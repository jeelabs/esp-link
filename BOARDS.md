Boards with esp-link
====================

This readme provides instructions for PCBs that I've made that are designed for esp-link.
Some of the instructions may be helpful to others as well.

esp-bridge
----------
![dsc_5127](https://cloud.githubusercontent.com/assets/39480/8509323/11037aa2-2252-11e5-9bd2-6c86c9a3b2ed.jpg)

The esp-bridge has an esp-03 modulde, an FTDI connector (with wrong pinout!), a 3-pin power
and debug connector, and two buttons.
Next to the buttons it is marked "TvE2015 esp-ftdi".
It comes preloaded with the latest version of esp-link.

Power: the on-board MCP1825S-33 regulator can provide 500mA and is good from about 3.6v to 6v.
Connect power either to the 3-pin connector (GND in center, 5v towards the esp module), or to
the FTDI connector (GND marked next to the buttons, 5V on 3rd pin).

On power-up you should see the green LED on for ~1 second (the yellow should go on too, but
the firmware may not be configured correctly). After that the green should blink according to the
patterns described in the README's LED indicators section. Follow the Wifi configuration details
section thereafter.

To connect a JeeNode to the esp-bridge to flash the AVR or debug it, plug it into the FTDI
port flipped-over, i.e. the component side of the JeeNode will be on the bottom and the
components of the esp-bridge will be on the top. (Yes, the FTDI port should have been reversed
on the esp-bridge...)

To program the JeeNode, having set-up the Wifi through the web pages, run avrdude with an
option like "-Pnet:esp8266:23" (you can use an IP address instead of `esp8266`). My test command
line is as follows:
```
/home/arduino/arduino-1.0.5/hardware/tools/avrdude \
  -C /home/arduino/arduino-1.0.5/hardware/tools/avrdude.conf -DV -patmega328p \
  -Pnet:esp8266:23 -carduino -b115200 -U flash:w:greenhouse.hex:i
```
If you're using "edam's Arduino makefile" then you can simply set `SERIALDEV=net:bbb:2000` in your
sketch's Makefile.

To program an LPC processor using the JeeLabs uploader. follow the instructions below for the jn-esp.

Reflashing the esp-bridge itself (as opposed to the attached uController):
_you should not need to do this!_, in general use the over-the-air reflashing by downloading the latest release.
If you cannot reflash over-the-air and need to reflash serially, connect TX of a
USB BUB to RX of the esp-bridge and RX to TX (i.e. cross-over). Hold the flash button down
and briefly press the reset button. Then run esptool.py.as described below.

jn-esp
-------
![dsc_5125](https://cloud.githubusercontent.com/assets/39480/8509322/08194674-2252-11e5-8539-4eacb0d79304.jpg)

The jn-esp has an esp-03 module, an LPC824, a pseudo-FTDI connector (marked in tiny letters)
and a JeePort (also marked). On the bottom it is marked "JN-ESP-V2".
It comes preloaded with the latest version of esp-link.

Power: the on-board MCP1825S-33 regulator can provide 500mA and is good from about 3.6v to 6v.
Connect power to the FTDI connector (GND and 5V marked on bottom).

On power-up you should see the green LED on for ~1 second (the yellow should go on too, but
the firmware may not be configured correctly). After that the green should blink according to the
patterns described in the README's LED indicators section. Follow the Wifi configuration details
section thereafter.

To program the LPC824 _ensure that you have a recent version of the Embello uploader_
and point the Embello uploader at port 23. Something like:
```
uploader -w -t -s 192.168.0.92:23 build/firmware.bin
```
Remove the -s option if you don't want to stay connected. A simple sketch to try this out
with is the [hello sketch](https://github.com/jeelabs/embello/tree/master/projects/jnp/hello).
The result should look something like:
```
$ uploader -w -t -s jn-esp:23 build/firmware.bin
found: 8242 - LPC824: 32 KB flash, 8 KB RAM, TSSOP20
hwuid: 16500407679C61AE7189A053830200F5
flash: 0640 done, 1540 bytes
entering terminal mode, press <ESC> to quit:


[hello]
500
1000
1500
2000
2500
...
```

The pseudo-ftdi connector has the following pin-out:
 - 1: GND (marked on bottom)
 - 2: LPC824 P17/A9
 - 3: 5V (marked on bottom)
 - 4: LPC824 P11/SDA
 - 5: LPC824 P10/SCL
 - 6: LCP824 P23/A3/C4

The JeePort connector has the following pin-out:
 - 1: LPC824 SWDIO/P2 (not 5v unlike JeeNodes!)
 - 2: LPC824 P14/A2/C3
 - 3: GND
 - 4: 3.3V (reg output)
 - 5: LPC824 P13/A10
 - 6: LPC824 SWCLK/P2

Reflashing the jn-esp's esp8266 itself (as opposed to the attached uController):
_you should not need to do this!_, in general use the over-the-air reflashing by downloading the latest release.
If you cannot reflash over-the-air and need to reflash serially, there are SMD pads for an FTDI connector on the
bottom of the PCB below the esp-03 module. GND is marked. The best is to solder a right-angle
connector to it such that the pins point up (i.e. to the component side). You can then
hook-up a USB-BUB. I recommend jumpering the flash pin (next to GND) to GND and to
hook the reset pin (6) to the USB-BUB's DTR (should happen automatically). RX&TX also go
straight through).

Wifi-link-12
------------

The wifi-link has an esp-12 modulde, an FTDI connector, and a 2-pin power connector.
The underside is marked "Wifi-link-12-v2 Jeelabs TvE2015".
It comes preloaded with the latest version of esp-link V2.

The ftdi connector has the following pin-out:
 - 1: GND (marked top&bottom)
 - 2: CTS (used as ISP to program ARM processors, also esp's TX when flashing the esp)
 - 3: 5V (marked on bottom)
 - 4: TX (from the esp)
 - 5: RX (to the esp)
 - 6: DTR (used ass reset to program AVR and ARM processors, also esp's RX when flashing the esp)

Power: the on-board LM3671 switching regulator can provide 600mA while staying cool and is good from about 3.6v to 5.5v!
Connect power to the marked power connector or to the FTDI connector (GND marked, 5V on 3rd pin). Do not exceed 5.5v!!

It is possible to bypass the on-board regulator and power the wifi-link directly with 3.3v: connect the 3.3v to the FTDI connector ("5v" pin) and switch the jumper on the bottom from 5v to 3v3 (you need to cut the tiny trace on the 5v side and jumper the two pads on the 3v3 side).

On power-up you should see the green LED on for ~1 second (the yellow should go on too, but
the firmware may not be configured correctly). After that the green should blink according to the
patterns described in the README's LED indicators section. Follow the Wifi configuration details
section thereafter.

The bottom also has a "sleep" jumper which connects the esp's reset pin with its gpio16 to enable deep-sleep
mode. This jumper is open and must be bridged to use the timed-wake-up feature of the esp's deep-sleep mode.

To connect a JeeNode to the esp-bridge to flash the AVR or debug it, plug it into the FTDI
port straight, i.e. the component side of the JeeNode and of the wifi-link will be on the top.

To connect an arduino, jumper gnd, tx, rx, and dtr for reset. The wifi-link-12-v2 has a 2.2K resistor on rx (serial going from AVR to esp) in order to protect it from 5v signals.

To program the JeeNode or AVR, having set-up the Wifi through the web pages, run avrdude with an
option like "-Pnet:esp-link:23" (you can use an IP address instead of `esp-link`). My test command
line is as follows:
```
/home/arduino/arduino-1.0.5/hardware/tools/avrdude \
  -C /home/arduino/arduino-1.0.5/hardware/tools/avrdude.conf -DV -patmega328p \
  -Pnet:esp-link:23 -carduino -b115200 -U flash:w:greenhouse.hex:i
```
If you're using "edam's Arduino makefile" then you can simply set `SERIALDEV=net:esp-link:23` in your
sketch's Makefile. You can also use port 2323 which forces programming mode (no real benefit with avrdude, but can
enable programming PICs).

Serially reflashing the wifi-link itself (as opposed to the attached uController):
_you should not need to do this!_, in general use the over-the-air reflashing by downloading the latest release.
If you cannot reflash over-the-air and need to reflash serially, follow this process:
- connect TX of the programmer (such as USB-BUB or a FDTI-friend) to DTR (FTDI pin 6)
- connect RX of the programmer to CTS (FTDI pin 2)
- short the flash jumper on the top of the board (push tweezers or a piece of wire into the jumper gap),
  the green LED will be on solid when you have contact
- briefly interrupt the power to reset the esp
- it will now be in flash mode: the green LED should be off (assuming you release the flash jumper),
  if you see the green LED come on it has rebooted into esp-link and you need to reset it again
  (the whole proess take some fiddling, you can solder a real jumper or switch to the flash pads and there's a similar
  reset jumper pad on the bottom of the PCB).
- use your favorite programming tool to reflash, you have time if the esp really entered programming mode, it often takes me
  2-3 tries until the programming works

Serial flashing
---------------

Once you have a version of esp-link flashed to your module or if you received a pre-flashed
module from me you should not need this section. But sometimes things fail badly and your
module is "brocked", this is how you receover.

### Installing esptool.py

On Linux I am using [esptool.py](https://github.com/themadinventor/esptool) to flash the esp8266.
If you're a little python challenged (like I am) then the following install instructions might help:
 - Install ez_setup with the following two commands (I believe this will do something
   reasonable if you already have it):

        wget https://bootstrap.pypa.io/ez_setup.py
        python ez_setup.py

 - Install esptool.py:

        git clone https://github.com/themadinventor/esptool.git
        cd esptool
        python setup.py install
        cd ..
        esptool.py -h

### Flashing esp-link

Using esptool.py a esp-link release can be flashed as follows:
```
curl -L https://github.com/jeelabs/esp-link/releases/download/0.10.1/esp-link.tgz | tar xzf -
cd esp-link
esptool.py write_flash 0x00000 boot_v1.4\(b1\).bin 0x1000 user1.bin 0x7e000 blank.bin
```
If you want to speed things up a bit and if you need to specify the port you can use a command
line like:
```
esptool.py --port /dev/ttyUSB0 --baud 460880 write_flash 0x00000 boot_v1.4\(b1\).bin \
           0x1000 user1.bin 0x7e000 blank.bin
```
