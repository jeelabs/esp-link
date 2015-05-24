<html>
<head><title>Help - ESP Link</title>
<link rel="stylesheet" type="text/css" href="style.css">
</head>
<body>
<div id="main">
<div id="topnav">%topnav%</div>
<h1><span class="esp">esp</span> link - Help</h1>

The ESP Link functions in two wifi modes: Station+AccessPoint (STA+AP) and Station (STA).
In the STA+AP mode it presents a network called esp8266 that you can connect to using the
password jeelabs8266. This mode is intended for initial configuration, but it is
fully functional. Typically the easiest way to connect to the esp8266 network is using a phone,
tablet, or laptop.</p>
<p>The recommended next step is to configure the ESP Link to connect to your "normal"
Wifi network so you can access it from any of your machines. Once you have connected the ESP Link
to your network and pointed your browser at it successfully, you should
switch the ESP Link to STA mode, which is more secure (no canned password).<p>
<p>In STA mode the ESP Link connects to the configured network (the info is saved in flash).
If, after a reset, it cannot connect for one minute, it automatically reverts to STA+AP mode
allowing you to reconnect to the esp8266 network to change configuration.</p>
<p>In STA mode the most tricky part usually is the IP address. On most networks, the ESP Link
will get a dynamic IP address assigned via DHCP and you now need to enter that IP address into
your browser to connect. The good news is that after you reset your ESP Link it will continue to
have the same IP address. However, if you leave it off for the week-end it will most likely get a
fresh IP the next time it starts up. On many Wifi routers you can enter a fixed mapping from
the ESP Link's hardware MAC address to a static IP address so it always gets the same IP
address. This is the recommended method of operation.</p>

<h1>Using your ESP Serial Programmer</h1>
The ESP Programmer can used in several distinct ways:
<ul>
<li>as a transparent bridge between TCP port 23 and the serial port</li>
<li>as a web console to see input from the serial port</li>
<li>as an Arduino, AVR, or ARM processor programmer using serial-over-TCP</li>
<li>as an Arduino, AVR, or ARM processor programmer by uploading HEX files (not yet functional)</li>
</ul>

<h2>Transparent bridge</h2>
<p>The ESP accepts TCP connections to port 23 and "connects" through to the serial port.
Up to 5 simultaneous TCP connections are supported and characters coming in on the serial
port get passed through to all connections. Characters coming in on a connection get copied
through to the serial port.</p>
<p>When using Linux a simple way to use this is <tt>nc esp8266 23</tt></p>

<h2>Programmer using serial-over-TCP</h2>
<p>By hooking up the ESP's GPIO lines to the reset line of an Arduino (or AVR in general) that is
preloaded with the Optiboot bootloader/flasher it is possible to reprogram these processors over
Wifi. The way is works is that the ESP toggles the reset line each time a connection is established
and the first characters are the flash programming synchronization sequence.</p>
<p>When using Linux avrdude can be instructed to program an AVR over TCP by using a special syntax
for the serial port: <tt>-Pnet:esp8266:23</tt>, where <tt>esp8266</tt> is the hostname of the ESP
Serial Programmer (an IP address could have been used instead).</p>
<p>NXP's LPC800-serial ARM processors can be programmed similarly by hooking up GPIO pins to the
ARM's reset and ISP lines. The ESP Serial Programmer issues the correct reset/isp pulses to put 
the ARM chip into firmware programming mode.</p>

<h2>Web Console</h2>
<p>The output of an attached Arduino/AVR/ARM can also be monitored via the console web page.
When connecting, it shows the most recent 10KB of characters received on the serial port and
then continues to print everything that comes in on the serial port. Eventually the page refreshes
when it gets very long. (Yes, this could be improved with some javascript...)</p>

<h2>Programmer using HEX upload</h2>
<p><i>(Not yet functional)</i> Instead of using the wifi-to-serial bridge to program
microcontrollers it is often faster to upload the HEX file to the ESP Serial Programmer and
have it perform the actual programming protocol.</p>

</div>
</body></html>
