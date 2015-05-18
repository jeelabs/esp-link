ESP-LINK
========

This firmware implements a transparent bridge between Wifi and serial using an ESP8266 module.
It also provides support for flash-programming Arduino/AVR microcontrollers as well as
LPC800-series ARM microcontrollers via the ESP8266.

The firmware includes a tiny HTTP server based on
[esphttpd](http://www.esp8266.com/viewforum.php?f=34)
with a simple web interface.

Hardware info
-------------
This firmware is designed for esp8266 modules which have most esp I/O pins available.
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
If you want to simply flash the provided firmware binary, you can use your favorite
ESP8266 flashing tool and flash the following:
- `boot_v1.3(b3).bin` from the official `esp_iot_sdk_v1.0.1` to 0x00000
- `blank.bin` from the official SDK to 0x7e000
- `./firmware/user1.bin` to 0x01000
Note that the firmware assumes a 512KB flash chip, which most of the esp-01 thru esp-11
modules appear to have.

Wifi configuration
---------------------
After you have serially flashed the module it will create a wifi access point (AP) with an
SSID of the form `ESP_012ABC` where 012ABC is a piece of the module's MAC address.
Using a laptop, phone, or tablet connect to this SSID and then open a browser pointed at
http://192.168.0.1, you should them see the esp-link web site.

Now configure the wifi. The typical desired configuration is for the esp-link to be a
station on your local wifi network so can communicate with it from all your computers.

To make this happen, navigate to the wifi page and hit the "change to STA+AP mode" button.
This will cause the esp8266 to restart and yo will have to wait 5-10 seconds until you can 
reconnect to the ESP_123ABC wifi network and refres the wifi settings page.

At this point you should see a list of detected networks on the web page and you can select
yours. Enter a password if your network is secure (recommended...) and hit the connect button.

You should now see that the esp-link has connected to your network and it should show you
its IP address. Write it down and then follow the provided link (you may have to switch your
laptop, phone, or tablet back to your network before you can actually connect).

At this point the esp-link will have switched to STA mode and be just a station on your
wifi network. These settings are stored in flash and thereby remembered through resets and
power cycles. They are also remembered when you flash new firmware. Only flashing `blank.bin`
as indicated above will reset the wifi settings.

There is a fail-safe, which is that after a reset (need details) the esp-link will revert
back to AP+STA mode and thus both present its ESP_012ABC-style network and try to connect to
the requested network, which will presumably not work or it wouldn't be in fail-safe mode
in the first place. You can then connect to the network and reconfigure the station part.


Building the firmware
---------------------
The firmware has been built using the [esp-open-sdk](https://github.com/pfalcon/esp-open-sdk)

You probably also need an UNIX-like system.

To manage the paths to all this, you can source a small shell fragment into your current session. For
example, I source a file with these contents:
export PATH=${PWD}/esp-open-sdk/xtensa-lx106-elf/bin:$PATH
export SDK_BASE=${PWD}/esp-open-sdk/sdk
export ESPTOOL=${PWD}/esptool/esptool
export ESPPORT=/dev/ttyUSB0
export ESPBAUD=460800

Actual setup of the SDK and toolchain is out of the scope of this document, so I hope this helps you
enough to set up your own if you haven't already. 

If you have that, you can clone out the source code:
git clone http://github.com/jeelabs/esp-link

This project makes use of heatshrink, which is a git submodule. To fetch the code:
cd esphttpd
git submodule init
git submodule update

Now, build the code:
make

Flashing the firmware
---------------------
This firmware supports over-the-air (OTA) flashing, so you do not have to deal with serial
flashing again after the initial one! The recommended way to flash is to use `make wiflash`,
which assumes that you set ESP_HOSTNAME to the hostname or IP address of your esp-link

The flashing, restart, and re-associating with your wireless network takes about 15 seconds
and is fully automatic. The 512KB flash are divided into two 236KB partitions allowing for new
code to be uploaded into one partition while running from the other. This is the official
OTA upgrade method supported by the SDK, except that the firmware is POSTed to the module
using curl as opposed to having the module download it from a cloud server.

