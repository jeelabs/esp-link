ESP-LINK: Wifi-Serial Bridge w/REST&MQTT
========================================

The esp-link firmware connects a micro-controller to the internet using an ESP8266 Wifi module.
It implements a number of features:

- transparent bridge between Wifi and serial, useful for debugging or inputting into a uC
- flash-programming attached Arduino/AVR microcontrollers and
  LPC800-series and other ARM microcontrollers via Wifi
- built-in stk500v1 programmer for AVR uC's: program using HTTP upload of hex file
- outbound REST HTTP requests from the attached micro-controller to the internet
- MQTT client pub/sub from the attached micro-controller to the internet
- serve custom web pages containing data that is dynamically pulled from the attached uC and
  that contain buttons and fields that are transmitted to the attached uC (feature not
  fully ready yet)

The firmware includes a tiny HTTP server based on
http://www.esp8266.com/viewforum.php?f=34[esphttpd]
with a simple web interface, many thanks to Jeroen Domburg for making it available!
The REST and MQTT functionality are loosely based on https://github.com/tuanpmt/espduino
but significantly rewritten and no longer protocol compatible, thanks to tuanpmt for the
inspiration!

Many thanks to https://github.com/brunnels[brunnels] for contributions in particular around
the espduino functionality.
Thanks to https://github.com/cskarai[cskarai] for the custom dynamic web page functionality
and to https://github.com/beegee-tokyo[beegee-tokyo] for lots of code documentation.
Thank you also to https://github.com/susisstrolch[susisstrolch] for the syslog feature,
https://github.com/bc547[bc547], and https://github.com/katast[katast] for
additional contributions. Esp-link is the work of many contributors!

Note that http://github.com/jeelabs/esp-link is the original esp-link software which has
notably been forked by arduino.org as https://github.com/arduino-org/Esp-Link[Esp-Link] and shipped
with the initial Arduino Uno Wifi. The JeeLabs esp-link has evolved significantly since the
fork and added cool new features as well as bug fixes.

### Quick links

In this document: [goals](#esp-link-goals), [uses](#esp-link-uses), [eye candy](#eye-candy),
[getting-started](#getting-started), [contact](#contact).
Separate documents: [hardware configuration](), [serial flashing](), [troubleshooting](),
[over-the-air flashing](), [building esp-link](), [flash layout](), [serial bridge](),
[flashing an attached uC](), [MQTT and outbound REST requests](), [service web pages]()

For quick support and questions chat at
image:https://badges.gitter.im/Join%20Chat.svg[link="https://gitter.im/jeelabs/esp-link"]
or (a little slower) open a github issue.

Releases & Downloads
--------------------
Esp-link uses semantic versioning. The main change between versions 1.x and 2.x was the
addition of MQTT and outbound REST requests from the attached uC. The main change between 2.x
and 3.x will be the addition of custom web pages (this is not ready yet).

- The master branch is currently unstable as we integrate a number of new features to get
  to version 3.0. Please use v2.2.3 unless you want to hack up the latest code!
  This being said, the older functionality seems to work fine on master, YMMV...
- https://github.com/jeelabs/esp-link/releases/tag/v2.2.3[V2.2.3] is the most recent release.
  It has a built-in stk500v1 programmer (for AVRs), work on all modules, and supports mDNS,
  sNTP, and syslog. It is built using the Espressif SDK 1.5.4.
- https://github.com/jeelabs/esp-link/releases/tag/v2.1.7[V2.1.7] is the previous release.
- See https://github.com/jeelabs/esp-link/releases[all releases].

Intro
-----

### Esp-link goals

The goal of the esp-link project is to create an advanced Wifi co-processor. Esp-link assumes that
there is a "main processor" (also referred to as "attached uController") and that esp-link's role
is to facilitate communication over Wifi. This means that esp-link does not just connect TCP/UDP
sockets through to the attached uC, rather it implements mostly higher-level functionality to
offload the attached uC, which often has much less flash and memory than esp-link.

Where esp-link is a bit unusual is that it's not really
just a Wifi interface or a slave co-processor. In some sense it's the master, because the main
processor can be reset, controlled and reprogrammed through esp-link. The three main areas of
functionality in esp-link are:

- reprogramming and debugging the attached uC
- letting the attached uC make outbound communication and offloading the protocol processing
- forwarding inbound communication and offloading the protocol processing

The goal of the project is also to remain focused on the above mission. In particular, esp-link
is not a platform for stand-alone applications and it does not support connecting sensors or
actuators directly to it. A few users have taken esp-link as a starting point for doing these
things and that's great, but there's also value in keeping the mainline esp-link project
focused on a clear mission.

### Esp-link uses

The simplest use of esp-link is as a transparent serial to wifi bridge. You can flash an attached
uC over wifi and you can watch the uC's serial debug output by connecting to port 23 or looking
at the uC Console web page.

The next level is to use the outbound connectivity of esp-link in the uC code. For example, the
uC can use REST requests to services like thingspeak.com to send sensor values that then get
stored and plotted by the external service.
The uC can also use REST requests to retrieve simple configuration
information or push other forms of notifications. (MQTT functionality is forthcoming.)

An additional option is to add code to esp-link to customize it and put all the communication
code into esp-link and only keep simple sensor/actuator control in the attached uC. In this
mode the attached uC sends custom commands to esp-link with sensor/acturator info and
registers a set of callbacks with esp-link that control sensors/actuators. This way, custom
commands in esp-link can receive MQTT messages, make simple callbacks into the uC to get sensor
values or change actuators, and then respond back with MQTT. The way this is architected is that
the attached uC registers callbacks at start-up such that the code in the esp doesn't need to 
know which exact sensors/actuators the attached uC has, it learns that through the initial
callback registration.

### Eye Candy

These screen shots show the Home page, the Wifi configuration page, the console for the
attached microcontroller, and the pin assignments card:

image:https://cloud.githubusercontent.com/assets/39480/8261425/6ca395a6-167f-11e5-8e92-77150371135a.png[width="45%"]
image:https://cloud.githubusercontent.com/assets/39480/8261427/6caf7326-167f-11e5-8085-bc8b20159b2b.png[width="45%"]
image:https://cloud.githubusercontent.com/assets/39480/8261426/6ca7f75e-167f-11e5-827d-9a1c582ad05d.png[width="45%"]
image:https://cloud.githubusercontent.com/assets/39480/8261658/11e6c64a-1681-11e5-82d0-ea5ec90a6ddb.png[width="45%"]

Getting Started
---------------

To get started you need to:
1. prepare your esp8266 module for serial flashing
2. download the latest esp-link release image (you can build your own later)
3. flash the firmware
4. configure the Wifi in esp-link for your network

You can then attach a uC and upload a sketch:
1. attach a uC (e.g. arduino) to your esp8266 module
2. connect via the serial port to see a pre-loaded sketch running
3. upload a fresh version of the sketch

From there, more advanced steps are:
- write a sketch that uses MQTT to communicate, or that makes outbound REST requests
- create some web pages and write a sketch that populates data in them or reacts to buttons
  and forms
- make changes or enhancements to esp-link and build your own firmware

Serial bridge and connections to Arduino, AVR, ARM, LPC microcontrollers
------------------------------------------------------------------------

In order to connect through the esp-link to a microcontroller use port 23. For example,
on linux you can use `nc esp-hostname 23` or `telnet esp-hostname 23`.

Note that multiple connections to port 23 and 2323 can be made simultaneously. Esp-link will
intermix characters received on all these connections onto the serial TX and it will
broadcast incoming characters from the serial RX to all connections. Use with caution!

### Flashing an attached AVR/Arduino

There are three options for reprogramming an attached AVR/Arduino microcontroller:

- Use avrdude and point it at port 23 of esp-link. Esp-link automatically detects the programming
  sequence and issues a reset to the AVR.
- Use avrdude and point it at port 2323 of esp-link. This is the same as port 23 except that the
  autodectection is not used and the reset happens because port 2323 is used
- Use curl or a similar tool to HTTP POST the firmware to esp-link. This uses the built-in
  programmer, which only works for AVRs/Arduinos with the optiboot bootloader (which is std).

To reprogram an Arduino / AVR microcontroller by pointing avrdude at port 23 or 2323 you
specify a serial port of the form `net:esp-link:23` in avrdude's -P option, where
`esp-link` is either the hostname of your esp-link or its IP address).
This is instead of specifying a serial port of the form /dev/ttyUSB0.
Esp-link detects that avrdude starts its connection with a flash synchronization sequence
and sends a reset to the AVR microcontroller so it can switch into flash programming mode.

To reprogram using the HTTP POST method you need to first issue a POST to put optiboot into
programming mode: POST to `http://esp-link/pgm/sync`, this starts the process. Then check that
synchronization with optiboot has been achieved by issuing a GET to the same URL
(`http://esp-link/pgm/sync`). Repeat until you have sync (takes <500ms normally). Finally
issue a POST request to `http://esp-link/pgm/upload` with your hex file as POST data (raw,
not url-encoded or multipart-mime. Please look into the avrflash script for the curl command-line
details or use that script directly (`./avrflash esp-link.local my_sketch.hex`).
_Important_: after the initial sync request that resets the AVR you have 10 seconds to get to the
upload post or esp-link will time-out. So if you're manually entering curl commands have them
prepared so you can copy&paste!

Beware of the baud rate, which you can set on the uC Console page. Sometimes you may be using
115200 baud in sketches but the bootloader may use 57600 baud. When you use port 23 or 2323 you
need to set the baud rate correctly. If you use the built-in programmer (HTTP POST method) then
esp-link will try the configured baud rate and also 9600, 57600, and 115200 baud, so it should
work even if you have the wrong baud rate configured...

When to use which method? If port 23 works then go with that. If you have trouble getting sync
or it craps out in the middle too often then try the built-in programmer with the HTTP POST.
If your AVR doesn't use optiboot then use port 2323 since esp-link may not recognize the programming
sequence and not issue a reset if you use port 23.

If you are having trouble with the built-in programmer and see something like this:

--------------------
# ./avrflash 192.168.3.104 blink.hex
Error checking sync: FAILED to SYNC: abandoned after timeout, got:
:\xF/\x00\xCj\xCz\xCJ\xCZ\xC\xAÜ\xC\xAä\xC\xAÜ\xC\xAä\xC\xBì\xC\xBô\xC\xBì\xC\xBô\xC\xAÜ\xC\xAä\xC
--------------------

the most likely cause is a baud rate mismatch and/or a bad connection from the esp8266 to the
AVRs reset line.
The baud rate used by esp-link is set on the uC Console web page and, as mentioned above, it will
automatically try 9600, 57600, and 115200 as well.
The above garbage characters are most likely due to optiboot timing out and starting the sketch
and then the sketch sending data at a different baud rate than configured into esp-link.
Note that sketches don't necessarily use the same baud rate as optiboot, so you may have the
correct baud rate configured but reset isn't functioning, or reset may be functioning but the
baud rate may be incorrect.

The output of a successful flash using the built-in programmer looks like this:

--------------------
Success. 3098 bytes at 57600 baud in 0.8s, 3674B/s 63% efficient
--------------------

This says that the sketch comprises 3098 bytes of flash, was written in 0.8 seconds
(excludes the initial sync time) at 57600 baud,
and the 3098 bytes were flashed at a rate of 3674 bytes per second.
The efficiency measure is the ratio of the actual rate to the serial baud rate,
thus 3674/5760 = 0.63 (there are 10 baud per character).
The efficiency is not 100% because there is protocol overhead (such as sync, record type, and
length characters)
and there is dead time waiting for an ack or preparing the next record to be sent.

### Details of built-in AVR flash algorithm

The built-in flashing algorithm differs a bit from what avrdude does. The programming protocol
states that STK_GET_SYNC+CRC_EOP (0x30 0x20) should be sent to synchronize, but that works poorly
because the AVR's UART only buffers one character. This means that if STK_GET_SYNC+CRC_EOP is
sent twice there is a high chance that only the last character (CRC_EOP) is actually
received. If that is followed by another STK_GET_SYNC+CRC_EOP sequence then optiboot receives
CRC_EOP+STK_GET_SYNC+CRC_EOP which causes it to abort and run the old sketch. Ending up in that
situation is quite likely because optiboot initializes the UART as one of the first things, but
then goes off an flashes an LED for ~300ms during which it doesn't empty the UART.

Looking at the optiboot code, the good news is that CRC_EOP+CRC_EOP can be used to get an initial
response without the overrun danger of the normal sync sequence and this is what esp-link does.
The programming sequence runs as follows:

- esp-link sends a brief reset pulse (1ms)
- esp-link sends CRC_EOP+CRC_EOP ~50ms later
- esp-link sends CRC_EOP+CRC_EOP every ~70-80ms
- eventually optiboot responds with STK_INSYNC+STK_OK (0x14;0x10)
- esp-link sends one CRC_EOP to sort out the even/odd issue
- either optiboot responds with STK_INSYNC+STK_OK or nothing happens for 70-80ms, in which case
  esp-link sends another CRC_EOP
- esp-link sends STK_GET_SYNC+CRC_EOP and optiboot responds with STK_INSYNC+STK_OK and we're in
  sync now
- esp-link sends the next command (starts with 'u') and programming starts...

If no sync is achieved, esp-link changes baud rate and the whole thing starts over with a reset
pulse about 600ms, esp-link gives up after about 5 seconds and reports an error.

### Flashing an attached ARM processor

You can reprogram NXP's LPC800-series and many other ARM processors as well by pointing your
programmer similarly at the esp-link's port 23. For example, if you are using
https://github.com/jeelabs/embello/tree/master/tools/uploader a command line like
`uploader -t -s -w esp-link:23 build/firmware.bin` does the trick.
The way it works is that the uploader uses telnet protocol escape sequences in order to
make esp-link issue the appropriate "ISP" and reset sequence to the microcontroller to start the
flash programming. If you use a different ARM programming tool it will work as well as long as
it starts the connection with the `?\r\n` synchronization sequence.

### Flashing an attached esp8266

Yes, you can use esp-link running on one esp8266 module to flash another esp8266 module,
however it is rather tricky! The problem is not electric, it is wifi interference.
The basic idea is to use some method to direct the esp8266 flash program to port 2323 of
esp-link. Using port 2323 with the appropriate wiring will cause the esp8266's reset and 
gpio0 pins to be toggled such that the chip enters the flash programming mode.

One option for connecting the programmer with esp-link is to use my version of esptool.py
at http://github.com/tve/esptool, which supports specifying a URL instead of a port. Thus
instead of specifying something like `--port /dev/ttyUSB0` or `--port COM1` you specify
`--port socket://esp-link.local:2323`. Important: the baud rate specified on the esptool.py
command-line is irrelevant as the baud rate used by esp-link will be the one set in the
uC console page. Fortunately the esp8266 bootloader does auto-baud detection. (Setting the
baud rate to 115200 is recommended.)

Another option is to use a serial-to-tcp port forwarding driver and point that to port 2323
of esp-link. On windows users have reported success with
http://www.hw-group.com/products/hw_vsp/hw_vsp2_en.html[HW Virtual Serial Port]

Now to the interference problem: once the attached esp8266 is reset it
starts outputting its 26Mhz clock on gpio0, which needs to be attached to
the esp8266 running esp-link (since it needs to drive gpio0 low during
the reset to enter flash mode). This 26Mhz signal on gpio0 causes a
significant amount of radio interference with the result that the esp8266
running esp-link has trouble receiving Wifi packets. You can observe this
by running a ping to esp-link in another window: as soon as the target
esp8266 is reset, the pings become very slow or stop altogetehr. As soon
as you remove power to the attached esp8266 the pings resume beautifully.

To try and get the interference under control, try some of the following:
add a series 100ohm resistor and 100pf capacitor to ground as close to
the gpio0 pin as possible (basically a low pass filter); and/or pass
the cable connecting the two esp8266's through a ferrite bead.

### Debug log

The esp-link web UI can display the esp-link debug log (os_printf statements in the code). This
is handy but sometimes not sufficient. Esp-link also prints the debug info to the UART where
it is sometimes more convenient and sometimes less... For this reason three UART debug log
modes are supported that can be set in the web UI (and the mode is saved in flash):

- auto: the UART log starts enabled at boot using uart0 and disables itself when esp-link
  associates with an AP. It re-enables itself if the association is lost.
- off: the UART log is always off
- on0: the UART log is always on using uart0
- on1: the UART log is always on using uart1 (gpio2 pin)

Note that even if the UART log is always off the ROM prints to uart0 whenever the
esp8266 comes out of reset. This cannot be disabled.

Outbound HTTP REST requests and MQTT client
-------------------------------------------

The V2 versions of esp-link use the SLIP protocol over the serial link to support simple outbound
HTTP REST requests as well as an MQTT client. The SLIP protocol consists of commands with
binary arguments sent from the
attached microcontroller to the esp8266, which then performs the command and responds back.
The responses back use a callback address in the attached microcontroller code, i.e., the
command sent by the uC contains a callback address and the response from the esp8266 starts
with that callback address. This enables asynchronous communication where esp-link can notify the
uC when requests complete or when other actions happen, such as wifi connectivity status changes.

You can find REST and MQTT libraries as well as demo sketches in the
https://github.com/jeelabs/el-client[el-client] repository.

Contact
-------

If you find problems with esp-link, please create a github issue. If you have a question, please
use the gitter chat link at the top of this page.
