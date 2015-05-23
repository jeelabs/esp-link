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
its IP address. _Write it down_ (due to a bug ou won't see it anymore after this) and then
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

In order to OTA-update the esp8266 you should "export ESP_HOSTNAME=..." with the hostname or
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

