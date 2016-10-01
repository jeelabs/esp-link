#!/usr/bin/perl

use strict;
use Data::Dumper;

my $dir = shift @ARGV;
my $out = shift @ARGV;

my $espfs = '';

my @structured = read_dir_structure($dir, "");

for my $file (@structured)
{
  my $flags = 0;
  my $name = $file;
  my $compression = 0;
  
  if( $name =~ /\.gz$/ )
  {
    $flags |= 2;
    $name =~ s/\.gz$//;
  }

  my $head = '<!doctype html><html><head><title>esp-link</title><link rel=stylesheet href="/pure.css"><link rel=stylesheet href="/style.css"><meta name=viewport content="width=device-width, initial-scale=1"><script src="/ui.js"></script><script src="/userpage.js"></script></head><body><div id=layout>';
  
  open IF, "<", "$dir/$file" or die "Can't read file: $!";
  my @fc = <IF>;
  close(IF);
  my $cnt = join("", @fc);

  if( $name =~ /\.html$/ )
  {
    if( ! ( $flags & 2 ) )
    {
      $cnt = "$head$cnt";
    }
    else
    {
      printf("TODO: prepend headers to GZipped HTML content!\n");
    }
  }
  
  $name .= chr(0);
  $name .= chr(0) while( (length($name) & 3) != 0 );
  
  my $size = length($cnt);
  
  $espfs .= "ESfs";
  $espfs .= chr($flags);
  $espfs .= chr($compression);
  $espfs .= chr( length($name) & 255 );
  $espfs .= chr( length($name) / 256 );
  $espfs .= chr( $size & 255 );
  $espfs .= chr( ( $size / 0x100 ) & 255 );
  $espfs .= chr( ( $size / 0x10000 ) & 255 );
  $espfs .= chr( ( $size / 0x1000000 ) & 255 );
  $espfs .= chr( $size & 255 );
  $espfs .= chr( ( $size / 0x100 ) & 255 );
  $espfs .= chr( ( $size / 0x10000 ) & 255 );
  $espfs .= chr( ( $size / 0x1000000 ) & 255 );

  $espfs .= $name;

  
  
  $cnt .= chr(0) while( (length($cnt) & 3) != 0 );
  $espfs .= $cnt;
}

$espfs .= "ESfs";
$espfs .= chr(1);
for(my $i=0; $i < 11; $i++)
{
  $espfs .= chr(0);
}

open FH, ">", $out or die "Can't open file for write, $!";
print FH $espfs;
close(FH);


exit(0);

sub read_dir_structure
{
  my ($dir, $base) = @_;

  my @files;
  
  opendir my $dh, $dir or die "Could not open '$dir' for reading: $!\n";

  while (my $file = readdir $dh) {
    if ($file eq '.' or $file eq '..') {
      next;
    }

    my $path = "$dir/$file";
    if( -d "$path" )
    {
      my @sd = read_dir_structure($path, "$base/$file");
      push @files, @sd ;
    }
    else
    {
      push @files, "$base/$file";
    }
  }

  close( $dh );
  
  $_ =~ s/^\/// for(@files);
  return @files;
}
