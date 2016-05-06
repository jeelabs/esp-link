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
 

my @webmethods = (
  [ "menu", \&getMenu ],
  [ "pins", \&getPins ],
  [ "system/info", \&getSystemInfo ],
  [ "wifi/info", \&getWifiInfo ],
);
 
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
       #print Dumper($httpReq);
       my $httpResp = process_http( $httpReq );
       #print Dumper($httpResp);

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

sub content_response
{
  my ($content, $url) = @_;
  
  my %resp;
  $resp{code} = 200;
  $resp{text} = "OK";
  $resp{done} = 1;
  $resp{body} = $content;
      
  $resp{fields} = {};
  $resp{fields}{'Content-Length'} = length($content);
      
  $resp{fields}{'Content-Type'} = "text/json";
  $resp{fields}{'Content-Type'} = "text/html; charset=UTF-8" if( $url =~ /\.html$/ );
  $resp{fields}{'Content-Type'} = "text/css" if( $url =~ /\.css$/ );
  $resp{fields}{'Content-Type'} = "text/javascript" if( $url =~ /\.js$/ );
  $resp{fields}{'Content-Type'} = "image/gif" if( $url =~ /\.ico$/ );
  $resp{fields}{'Connection'} = 'close';
      
  return \%resp;
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
      return content_response($cnt, $url); 
    }
    if( -f "$pth/web-server/$url" )
    {
      my $cnt = slurp( "$pth/web-server/$url" );
      
      if( $url =~ /\.html$/ )
      {
        my $prep = slurp( "$pth/head-user-" );
        $cnt = "$prep$cnt";
      }
      return content_response($cnt, $url); 
    }
    elsif( grep { $_->[0] eq $url } @webmethods )
    {
      my @mth = grep { $_->[0] eq $url } @webmethods;
      my $webm = $mth[0];
      
      return content_response( $webm->[1]->(), $url );
    }
    else
    {
      return error_response(404, "File not found");
    }
  }
  
  # TODO
  
  return error_response(400, "Invalid HTTP request");
}

sub getMenu
{
  my $out = sprintf(
    "{ " .
      "\"menu\": [ " .
        "\"Home\", \"/home.html\", " .
        "\"WiFi Station\", \"/wifi/wifiSta.html\", " .
        "\"WiFi Soft-AP\", \"/wifi/wifiAp.html\", " .
        "\"&#xb5;C Console\", \"/console.html\", " .
        "\"Services\", \"/services.html\", " .
#ifdef MQTT
        "\"REST/MQTT\", \"/mqtt.html\", " .
#endif
        "\"Debug log\", \"/log.html\", " .
        "\"Web Server\", \"/web-server.html\"" .
	"%s" .
      " ], " .
      "\"version\": \"%s\", " .
      "\"name\": \"%s\"" .
    " }", readUserPages(), "dummy", "dummy-esp-link");

  return $out;
}

sub getPins
{
  return '{ "reset":12, "isp":-1, "conn":-1, "ser":2, "swap":0, "rxpup":1 }';
}

sub getSystemInfo
{
  return '{ "name": "esp-link-dummy", "reset cause": "6=external", "size": "4MB:512/512", "upload-size": "3145728", "id": "0xE0 0x4016", "partition": "user2.bin", "slip": "disabled", "mqtt": "disabled/disconnected", "baud": "57600", "description": "" }';
}

sub getWifiInfo
{
  return '{"mode": "STA", "modechange": "yes", "ssid": "DummySSID", "status": "got IP address", "phy": "11n", "rssi": "-45dB", "warn": "Switch to <a href=\"#\" onclick=\"changeWifiMode(3)\">STA+AP mode</a>",  "apwarn": "Switch to <a href=\"#\" onclick=\"changeWifiMode(3)\">STA+AP mode</a>", "mac":"12:34:56:78:9a:bc", "chan":"11", "apssid": "ESP_012345", "appass": "", "apchan": "11", "apmaxc": "4", "aphidd": "disabled", "apbeac": "100", "apauth": "OPEN","apmac":"12:34:56:78:9a:bc", "ip": "192.168.1.2", "netmask": "255.255.255.0", "gateway": "192.168.1.1", "hostname": "esp-link", "staticip": "0.0.0.0", "dhcp": "on"}';
}

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

sub readUserPages
{
  my $pth = dirname $0;
  my @files = read_dir_structure( "$pth/web-server", "/" );
  
  @files = grep { $_ =~ /\.html$/ } @files;
  
  my $add = '';
  for my $f ( @files )
  {
    my $nam = $f;
    $nam =~ s/\.html$//;
    $nam =~ s/[^\/]*\///g;
    $add .= ", \"$nam\", \"$f\"";
  }
  
  return $add;
}
