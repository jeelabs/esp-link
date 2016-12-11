Flashing an attached Microcontroller
====================================

In order to connect through the esp-link to a microcontroller use port 23. For example,
on linux you can use `nc esp-hostname 23` or `telnet esp-hostname 23`.

Note that multiple connections to port 23 and 2323 can be made simultaneously. Esp-link will
intermix characters received on all these connections onto the serial TX and it will
broadcast incoming characters from the serial RX to all connections. Use with caution!

### Flashing an attached AVR/Arduino

There are multiple options for reprogramming an attached AVR/Arduino microcontroller:

- Use avrdude and point it at port 23 of esp-link. Esp-link automatically detects the programming
  sequence and issues a reset to the AVR.
- Use avrdude and point it at port 2323 of esp-link. This is the same as port 23 except that the
  autodectection is not used and the reset happens because port 2323 is used
- Use curl or a similar tool to HTTP POST the firmware to esp-link. This uses the built-in
  programmer, which only works for AVRs/Arduinos with the optiboot bootloader (which is std).
- Use some serial port forwarding software, such as com2com, or hwvsp (you have to uncheck
  nvt in the settings when using the latter).

To reprogram an Arduino / AVR microcontroller by pointing avrdude at port 23 or 2323 you
specify a serial port of the form `net:esp-link:23` in avrdude's -P option, where
`esp-link` is either the hostname of your esp-link or its IP address).
This is instead of specifying a serial port of the form /dev/ttyUSB0.
Esp-link detects that avrdude starts its connection with a flash synchronization sequence
and sends a reset to the AVR microcontroller so it can switch into flash programming mode.

Note for Windows users: very recent avrdude versions on Windows support the -P option, while older
ones don't. See the second-to-last bullet in the
[avrdude 6.3 release notes]http://savannah.nongnu.org/forum/forum.php?forum_id=8461).

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

```
# ./avrflash 192.168.3.104 blink.hex
Error checking sync: FAILED to SYNC: abandoned after timeout, got:
:\xF/\x00\xCj\xCz\xCJ\xCZ\xC\xAÜ\xC\xAä\xC\xAÜ\xC\xAä\xC\xBì\xC\xBô\xC\xBì\xC\xBô\xC\xAÜ\xC\xAä\xC
```

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

```
Success. 3098 bytes at 57600 baud in 0.8s, 3674B/s 63% efficient
```

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

__Flashing another esp8266 module is possible in theory but real-world attempts have so far been
rather unsuccessful due to Wifi interference. This section is left here in case someone else
wants to dig in and find a solution.__

You can use esp-link running on one esp8266 module to flash another esp8266 module,
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
