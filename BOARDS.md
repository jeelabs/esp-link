Boards with ESP-Link
====================

This readme provides instructions for PCBs that I've made that are designed for esp-link.
Some of the instructions may be helpful to others as well.

Installing esptool.py
---------------------

On Linux I am using [esptool.py](https://github.com/themadinventor/esptool) to flash the esp8266.
If you're a little python challenged (like I am) then the following install instructions might help:
 - Install ez_setup with the following two commands (I believe this will do something reasonable if you already have it):

        wget https://bootstrap.pypa.io/ez_setup.py
        python ez_setup.py
 - Install esptool.py:

        git clone https://github.com/themadinventor/esptool.git
        cd esptool
        python setup.py install
        cd ..
        esptool.py -h

Flashing esp-link
-----------------

Using esptool.py an esp-link release can be flashed as follows:
```
mkdir firmware
cd firmware
wget https://github.com/jeelabs/esp-link/releases/download/0.9.3/blank.bin
wget https://github.com/jeelabs/esp-link/releases/download/0.9.3/boot_v1.3.b3.bin
wget https://github.com/jeelabs/esp-link/releases/download/0.9.3/user1.bin
esptool.py write_flash 0x00000 boot_v1.3.b3.bi 0x1000 user1.bin 0x7e000 blank.bin
```
(I see that I need to produce a tgz download...)

esp-bridge
----------

The esp-bridge 


