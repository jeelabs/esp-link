#!/usr/bin/perl
use strict;

use IO::Socket::INET;
use Data::Dumper;
use File::Basename;

# auto-flush on socket
$| = 1;
 
# creating a listening socket
my $server = new IO::Socket::INET (
    LocalHost => '0.0.0.0',
    LocalPort => '7777',
    Proto => 'tcp',
    Listen => 5,
    Reuse => 1
);
die "cannot create socket $!\n" unless $server;
print "server waiting for client connection on port 7777\n";
 

my $client;

while ($client = $server->accept())
{
   my $pid ;
   while (not defined ($pid = fork()))
   {
     sleep 5;
   }
   if ($pid)
   {
       close $client;        # Only meaningful in the client 
   }
   else
   {
       $client->autoflush(1);    # Always a good idea 
       close $server;
       
       my $httpReq = parse_http( $client );
       print Dumper($httpReq);
       my $httpResp = process_http( $httpReq );
       print Dumper($httpResp);

       my $data = "HTTP/1.1 " . $httpResp->{code} . " " . $httpResp->{text} . "\r\n";
       
       if( exists $httpResp->{fields} )
       {
         for my $key( keys %{$httpResp->{fields}} )
         {
           $data .= "$key: " . $httpResp->{fields}{$key} . "\r\n";
         }
       }
       $data .= "\r\n";
       if( exists $httpResp->{body} )
       {
         $data .= $httpResp->{body};
       }
 
 print "$data\n\n";
       $client->send($data);
 
       if( $httpResp->{done} )
       {
         # notify client that response has been sent
         #shutdown($client, 1);
       }
   }
}

exit(0);

sub parse_http
{
  my ($client) = @_;
    # read up to 1024 characters from the connected client
    my $data = "";
    
    do{
      my $buf = "";
      $client->recv($buf, 1024);
      $data .= $buf;
    }while( $data !~ /\r\n\r\n/s );
    #print "Query: $data\n";
 
    my %resp;
    
    my @lines = split /\r\n/, $data;
    my $head = shift @lines;
    
    if( $head =~ /(GET|POST) / )
    {
      $resp{method} = $1;
      $head =~ s/(GET|POST) //;
      if( $head =~ /^([^ ]+) HTTP\/\d\.\d/ )
      {
        $resp{url} = $1;
        
        my %fields;
        while( my $arg = shift @lines )
        {
          if( $arg =~ /^([\w-]+): (.*)$/ )
          {
            $fields{$1} = $2;
          }
        }
        $resp{fields} = \%fields;
      }
      else
      {
        $resp{method} = 'ERROR';
        $resp{error} = 'Invalid HTTP request';
      }
    }
    else
    {
      $resp{method} = 'ERROR';
      $resp{error} = 'Invalid HTTP request';
    }
    
    return \%resp;
}

sub error_response
{
  my ($code, $msg) = @_;

  my %resp;
  $resp{code} = $code;
  $resp{text} = $msg;
  $resp{fields} = {};
  $resp{done} = 1;
  
  return \%resp;
}

sub slurp
{
  my ($file) = @_;
  
  open IF, "<", $file or die "Can't read file: $!";
  my @fc = <IF>;
  close(IF);
  my $cnt = join("", @fc);
  return $cnt;
}

sub process_http
{
  my ($httpReq) = @_;
  if( $httpReq->{method} eq 'ERROR' )
  {
    return error_response(400, $httpReq->{error});
  }
  
  if( $httpReq->{method} eq 'GET' )
  {
    my $url = $httpReq->{url};
    $url =~ s/^\///;
    
    $url = "home.html" if ! $url;

    my $pth = dirname $0;
    
    if( -f "$pth/../html/$url" )
    {
      my $cnt = slurp( "$pth/../html/$url" );
      
      if( $url =~ /\.html$/ )
      {
        my $prep = slurp( "$pth/../html/head-" );
        $cnt = "$prep$cnt";
      }
      
      my %resp;
      $resp{code} = 200;
      $resp{text} = "OK";
      $resp{done} = 1;
      $resp{body} = $cnt;
      
      $resp{fields} = {};
      $resp{fields}{'Content-Length'} = length($cnt);
      
      $resp{fields}{'Content-Type'} = "text/html; charset=UTF-8" if( $url =~ /\.html$/ );
      $resp{fields}{'Content-Type'} = "text/css" if( $url =~ /\.css$/ );
      $resp{fields}{'Content-Type'} = "text/javascript" if( $url =~ /\.js$/ );
      $resp{fields}{'Content-Type'} = "image/gif" if( $url =~ /\.ico$/ );
      $resp{fields}{'Connection'} = 'close';
      
      return \%resp;
    }
    else
    {
      return error_response(404, "File not found");
    }
  }
  
  # TODO
  
  return error_response(400, "Invalid HTTP request");
}
