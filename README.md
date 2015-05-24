ESP-LINK
========

This firmware implements a transparent bridge between Wifi and serial using an ESP8266 module.
It also provides support for flash-programming Arduino/AVR microcontrollers as well as
LPC800-series ARM microcontrollers via the ESP8266.

The firmware includes a tiny HTTP server based on
[esphttpd](http://www.esp8266.com/viewforum.php?f=34)
with a simple web interface, many thanks to Jeroen Domburg for making it available!

_WARNING: this project is still in development, don't expect it to work for you_

Hardware info
-------------
This firmware is designed for esp8266 modules which have most ESP I/O pins available.
The expected connections are:
- URXD: connect to TX of microcontroller
- UTXD: connect to RX of microcontroller
- GPIO12: connect to RESET of microcontroller
- GPIO13: connect to ISP of LPC/ARM microcontroller (not used with Arduino/AVR)
- GPIO0: optionally connect green "conn" LED to 3.3V (indicates wifi status)
- GPIO2: optionally connect yellow "ser" LED to 3.3V (indicates serial activity)

If you are using an FTDI connector, GPIO12 goes to DTR and GPIO13 goes to CTS

Initial flashing
----------------
(This is not necessary if you receive one of the jn-esp or esp-bridge modules.)
If you want to simply flash the provided firmware binary, you can use your favorite
ESP8266 flashing tool and flash the following:
- `boot_v1.3(b3).bin` from the official `esp_iot_sdk_v1.0.1` to 0x00000
- `blank.bin` from the official SDK to 0x7e000
- `./firmware/user1.bin` to 0x01000
Note that the firmware assumes a 512KB flash chip, which most of the esp-01 thru esp-11
modules appear to have.

Wifi configuration overview
------------------
The end state is to have the esp8266 join your pre-existing wifi network as a pure station.
However, in order to get there the esp8266 will start out as an access point and you'll have
to join its network to configure it. The short version is:
 1. the esp-link creates a wifi access point
 2. your laptop joins as a station and you configure the esp-link wifi with your network info
    by pointing your browser at `http://192.168.4.1/`
 3. the esp-link joins your network while continuing to also be an access point ("AP+STA")
 4. the esp-link succeeds in connecting and shuts down its own access point
 5. if the esp-link looses your network it brings up its access point again

LED indicators
--------------
Assuming the above LED configuration, the green LED will show the wifi status as follows:
- Very short flash once a second: not connected to a network and running as AP+STA
- Very short flash once every two seconds: not connected to a network and running as AP-only
- Even on/off at 1HZ: connected to your network but no IP address (waiting for DHCP)
- Steady on with very short off every 3 seconds: connected to your network with an IP address
  (esp-link shuts down its AP after 15 seconds)

The yellow LED will blink briefly every time serial data is sent or received by the esp-link.
(This does not function yet.)

Wifi configuration details
--------------------------
After you have serially flashed the module it will create a wifi access point (AP) with an
SSID of the form `ESP_012ABC` where 012ABC is a piece of the module's MAC address.
Using a laptop, phone, or tablet connect to this SSID and then open a browser pointed at
`http://192.168.4.1/`, you should then see the esp-link web site.

Now configure the wifi. The desired configuration is for the esp-link to be a
station on your local wifi network so you can communicate with it from all your computers.

To make this happen, navigate to the wifi page and you should see the esp-link scan
for available networks.
If nothing happens verify that it is in AP+STA mode and not in AP-only mode (I need to fix this).

You should then see a list of detected networks on the web page and you can select
yours. Enter a password if your network is secure (recommended...) and hit the connect button.

You should now see that the esp-link has connected to your network and it should show you
its IP address. _Write it down_ (due to a bug you won't see it anymore after this) and then
follow the provided link (you will have to switch your
laptop, phone, or tablet back to your network before you can actually connect).

At this point the esp-link will have switched to STA mode and be just a station on your
wifi network. These settings are stored in flash and thereby remembered through resets and
power cycles. They are also remembered when you flash new firmware. Only flashing `blank.bin`
as indicated above will reset the wifi settings.

There is a fail-safe, which is that after a reset or a configuration change, if the esp-link
cannot connect to your network it will revert back to AP+STA mode after 15 seconds and thus
both present its `ESP_012ABC`-style network and continue trying to reconnect to the requested network.
You can then connect to the esp-link's AP and reconfigure the station part.

Building the firmware
---------------------
The firmware has been built using the [esp-open-sdk](https://github.com/pfalcon/esp-open-sdk)
on a Linux system. Create an esp8266 directory, install the esp-open-sdk into a sub-directory.
Download the Espressif SDK (1.0.1) and also expand it into a sub-directory. Then clone
this repository into a third sub-directory.  This way the relative paths in the Makefile will work.
If you choose a different directory structure look at the Makefile for the appropriate environment
variables to define. (I have not used the esptool for flashing, so I don't know whether what's
in the Makefile for that works or not.)

In order to OTA-update the esp8266 you should `export ESP_HOSTNAME=...` with the hostname or
IP address of your module.

This project makes use of heatshrink, which is a git submodule. To fetch the code:
```
cd esp-link
git submodule init
git submodule update
```

Now, build the code: `make` in the top-level of esp-link.

Flashing the firmware
---------------------
This firmware supports over-the-air (OTA) flashing, so you do not have to deal with serial
flashing again after the initial one! The recommended way to flash is to use `make wiflash`
if you are also building the firmware.
If you are downloading firmware binaries use `./wiflash.sh`.
`make wiflash` assumes that you set `ESP_HOSTNAME` to the hostname or IP address of your esp-link.

The flashing, restart, and re-associating with your wireless network takes about 15 seconds
and is fully automatic. The 512KB flash are divided into two 236KB partitions allowing for new
code to be uploaded into one partition while running from the other. This is the official
OTA upgrade method supported by the SDK, except that the firmware is POSTed to the module
using curl as opposed to having the module download it from a cloud server.

If you are downloading the binary versions of the firmware (links forthcoming) you need to have
both `user1.bin` and `user2.bin` handy and run `wiflash.sh <esp-hostname> user1.bin user2.bin`.
This will query the esp-link for which file it needs, upload the file, and then reconnect to
ensure all is well.

Note that when you flash the firmware the wifi settings are all preserved so the esp-link should
reconnect to your network within a few seconds and the whole flashing process should take 15-30
from beginning to end. If you need to clear the wifi settings you need to reflash the `blank.bin`
using the serial port.

Serial bridge and connections to Arduino, AVR, ARM, LPC microcontrollers
------------------------------------------------------------------------
In order to connect through the esp-link to a microcontroller use port 23. For example,
on linux you can use `nc esp-hostname 23` or `telnet esp-hostname 23`.

You can reprogram an Arduino / AVR microcontroller by pointing avrdude at port 23. Instead of
specifying a serial port of the form /dev/ttyUSB0 use `net:esp-hostname:23` (where `esp-hostname`
is either the hostname of your esp-link or its IP address). The esp-link detects that avrdude
starts its connection with a flash synchronization sequence and sends a reset to the AVR
microcontroller so it can switch into flash programming mode.

You can reprogram NXP's LPC800-series ARM processors as well by pointing your programmer
similarly at the esp-link's port 23. For example, if you are using
https://github.com/jeelabs/embello/tree/master/tools/uploader a command line like
`uploader -s -w esp-link:23 build/firmware.bin` should do the trick.
The way it works is that esp-link detects that the uploader starts its connection with the
flash synchronization sequence `?\r\n` and issues the appropriate "ISP" and reset sequence
to the microcontroller to start the flash programming.

Note that multiple connections to port 23 can be made simultaneously. The esp-link will
intermix characters received on all these connections onto the serial TX and it will
broadcast incoming characters from the serial RX to all connections. Use with caution...

Flash layout
------------

The flash layout dictated by the bootloader is the following (all this assumes a 512KB flash chip
and is documented in Espressif's `99C-ESP8266__OTA_Upgrade__EN_v1.5.pdf`):
 - @0x00000 4KB bootloader
 - @0x01000 236KB partition1
 - @0x3E000 16KB user parameters
 - @0x40000 4KB unused
 - @0x41000 236KB partition2
 - @0x4E000 16KB system parameters

What this means is that we can flash just about anything into partition1 or partition2 as long
as it doesn't take more than 236KB and has the right format that the boot loader understands.
We can't mess with the first 4KB nor the last 16KB of the flash.
I have not investigated how badly the user
parameters and unused sections can be abused, that's for the moment where I get desperate looking
for a few more KB...

Now how does a code partition break down? that is reflected in the following definition found in
the loader scripts:
```
  dram0_0_seg :                         org = 0x3FFE8000, len = 0x14000
  iram1_0_seg :                         org = 0x40100000, len = 0x8000
  irom0_0_seg :                         org = 0x40201010, len = 0x2B000
```
This means that 80KB (0x14000) are reserved for "dram0_0", 32KB (0x8000) for "iram1_0" and
172KB (0x2B000) are reserved for irom0_0. The segments are used as follows:
 - dram0_0 is the data RAM and some of that gets initialized at boot time from flash (static variable initialization)
 - iram1_0 is the instruction RAM and all of that gets loaded at boot time from flash
 - irom0_0 is the instruction cache which gets loaded on-demand from flash (all functions
   with the `ICACHE_FLASH_ATTR` attribute go there)

You might notice that 80KB+32KB+172KB is more than 236KB and that's because not the entire dram0_0
segment needs to be loaded from flash, only the portion with statically initialized data.
You might also notice that while iram1_0 is as large as the chip's instruction RAM (at least
according to the info I've seen) the size of the irom0_0 segment is smaller than it could be,
since it's really not bounded by any limitation of the processor (it simply backs the cache).

When putting the OTA flash process together I ran into loader issues, namely, while I was having
relatively little initialized data and also not 32KB of iram1_0 instructions I was overflowing
the allotted 172KB of irom0_0. To fix the problem the build process modifies the loader scripts
(see the `build/eagle.esphttpd1.v6.ld` target in the Makefile) to increase the irom0_0 segment
to 224KB (a somewhat arbitrary value). This doesn't mean that there will be 224KB of irom0_0
in flash, it just means that that's the maximum the linker will put there without giving an error.
In the end what has to fit into the magic 236KB is the sum of the initialized data, the iram1_0
segment, and the irom0_0 segment, and the dram0_0 and iram1_0 segments can't exceed what's specified
in the loader script 'cause those are the limitations of the processor.

Now that you hopefully understand the above you can understand the line printed by the Makefile
when linking the firmware, which looks something like:
```
** user1.bin uses 218592 bytes of 241664 available
```
Here 241664 is 236KB and 218592 is the size of what's getting flashed, so you can tell that you have
another 22KB to spend (modulo some 4KB flash segment rounding).
The Makefile also prints a few more details:
```
ls -ls eagle*bin
  4 -rwxrwxr-x 1 tve tve   2652 May 24 10:12 eagle.app.v6.data.bin
176 -rwxrwxr-x 1 tve tve 179732 May 24 10:12 eagle.app.v6.irom0text.bin
  8 -rwxrwxr-x 1 tve tve   5732 May 24 10:12 eagle.app.v6.rodata.bin
 32 -rwxrwxr-x 1 tve tve  30402 May 24 10:12 eagle.app.v6.text.bin
```
This says that we have 179732 bytes of irom0_0, we have 5732+2652 bytes of dram0_0 (read-only data
plus initialized read-write data), and we have 30402 bytes of iram1_0.

There's an additional twist to all this for the espfs "file system" that esphttpd uses.
The data for this is loaded at the end of irom0_0 and is called espfs.
The Makefile modifies the loader script to place the espfs at the start of irom0_0 and
ensure that it's 32-bit aligned. The size of the espfs is shown here:
```
4026be14 g       .irom0.text    00000000 _binary_espfs_img_end
40269e98 g       .irom0.text    00000000 _binary_espfs_img_start
00001f7c g       *ABS*  00000000 _binary_espfs_img_size
```
Namely, 0x1f7c = 8060 bytes.


