Flashing esp-link
=================

### Hardware configuration for normal operation

This firmware is designed for any esp8266 module.
The recommended connections for an esp-01 module are:

- URXD: connect to TX of microcontroller
- UTXD: connect to RX of microcontroller
- GPIO0: connect to RESET of microcontroller
- GPIO2: optionally connect green LED to 3.3V (indicates wifi status)

The recommended connections for an esp-12 module are:

- URXD: connect to TX of microcontroller
- UTXD: connect to RX of microcontroller
- GPIO12: connect to RESET of microcontroller
- GPIO13: connect to ISP of LPC/ARM microcontroller (not used with Arduino/AVR)
- GPIO0: either a 1k-10k pull-up resistor to 3.3v or a green "conn" LED via a 1k-2.2k
  resistor to 3.3V (indicates wifi status)
- GPIO2: either a 1k-10k pull-up resistor to 3.3v or a yellow "ser" LED via a 1k-2.2k
  resistor to 3.3V (indicates serial activity)

At boot time the esp8266 ROM outputs a boot message on UTXD, this can cause problems to the attached
microcontroller. If you need to avoid this, you can configure esp-link to swap the uart pins.
You should then connect the esp-12 module as follows and choose the "swap_uart" pin assignment
in the esp-link web interface:

- GPIO13: connect to TX of microcontroller
- GPIO15: connect to RX of microcontroller and use a pull-down to ensure proper booting
- GPIO12: connect to RESET of microcontroller
- GPIO14: connect to ISP of LPC/ARM microcontroller (not used with Arduino/AVR)
- GPIO0: either a 1k-10k pull-up resistor to 3.3v or a green "conn" LED via a 1k-2.2k
  resistor to 3.3V (indicates wifi status)
- GPIO2: either a 1k-10k pull-up resistor to 3.3v or a yellow "ser" LED via a 1k-2.2k
  resistor to 3.3V (indicates serial activity)

The GPIO pin assignments can be changed dynamically in the web UI and are saved in flash.

### Hardware configuration for flashing

To flash firmware onto the esp8266 via the serial port the following must be observed:
- GPIO0 must be low when reset ends to put the esp8266 into flash programming mode, it must be high
  to enter normal run mode
- GPIO2 must be high (pull-up resistor)
- GPIO15 must be low (pull-down resistor)

### Initial serial flashing

Download the latest [release](https://github.com/jeelabs/esp-link/releases) or use the
`user1.bin` file that is produced by the build process.
You will need to flash the bootloader, the `user1.bin` firmware, blank wifi settings, and init data
as described below.

_Important_: the firmware adapts to the size of the flash chip using information
stored in the boot sector (address 0). This is the standard way that the esp8266 SDK detects
the flash size. What this means is that you need to set this properly when you flash the bootloader.
If you use esptool.py you can do it using the -ff and -fs options. See the end of this page for
instructions on installing esptool.py.

The short version for the serial flashing is:
- flash `boot_v1.X.bin` from the official SDK or from the release tgz to `0x00000`
- flash `blank.bin` from the official SDK or from the tgz to `0x3FE000`
- flash `esp_init_data_default.bin` from the official SDK or from the tgz to `0x3FC000`
- flash `user1.bin` to `0x01000`
- be sure to use the commandline flags to set the correct flash size when flashing the bootloader
- some of the addresses vary with flash chip size

After the initial flashing if you want to update the firmware it is recommended to use the
over-the-air update described further down. If you want to update serially you only need to
reflash `user1.bin`.

### 32Mbit / 4Mbyte module
On Linux using esptool.py this turns into the following for a 32mbit=4MByte flash chip,
such as an esp-12 module typically has (_substitute the appropriate release number and bootloader
version number_):
```
curl -L https://github.com/jeelabs/esp-link/releases/download/v2.2.3/esp-link-v2.2.3.tgz | \
    tar xzf -
cd esp-link-v2.2.3
esptool.py --port /dev/ttyUSB0 --baud 230400 write_flash -fs 32m -ff 80m \
    0x00000 boot_v1.5.bin 0x1000 user1.bin \
    0x3FC000 esp_init_data_default.bin 0x3FE000 blank.bin
```
I use a high baud rate as shown above because I'm impatient, but that's not required. Attention: For some modules you have to set the flash mode to `dio` by adding `--fm dio` to the command line above, otherwise they won't boot. 

### 4Mbit / 512Kbyte module
```
curl -L https://github.com/jeelabs/esp-link/releases/download/v2.2.3/esp-link-v2.2.3.tgz | \
    tar xzf -
cd esp-link-v2.2.3
esptool.py --port /dev/ttyUSB0 --baud 460800 write_flash -fs 4m -ff 40m \
    0x00000 boot_v1.5.bin 0x1000 user1.bin \
    0x7C000 esp_init_data_default.bin 0x7E000 blank.bin
```
The `-fs 4m -ff40m` options say 4Mbits and 40Mhz as opposed to 32Mbits at 80Mhz for the 4MByte
flash modules. Note the different address for esp_init_data_default.bin and blank.bin
(the SDK stores its wifi settings near the end of flash, so it changes with flash size).

For __8Mbit / 1MByte__ modules the addresses are 0xFC000 and 0xFE000.

Debian, and probably other Linux distributions, come with a different esptool. It is similar,
but all the flags are different. Here is an example of flashing an **ESP-01S** which has 1M of flash using
```
esptool -cp /dev/ttyUSB0 -cb 460800 -cd none -bz 1M\
        -ca 0x00000 -cf boot_v1.7.bin\
        -ca 0x01000 -cf user1.bin\
        -ca 0xFC000 -cf esp_init_data_default.bin\
        -ca 0xFE000 -cf blank.bin
```

__Warning__: there is a bug in boot_v1.5.bin which causes it to only boot into user1 once.
If that fails it gets stuck trying to boot into user2. If this happens (can be seen in the
boot output on uart2 at 76600 baud) reflash just blank.bin at 0x7E000 (4Mbit module). (Sigh)

## Updating the firmware over-the-air

This firmware supports over-the-air (OTA) flashing for modules with 1MByte or more flash,
so you do not have to deal with serial flashing again after the initial one!
The recommended way to flash is to use `make wiflash`
if you are also building the firmware and `./wiflash` if you are downloading firmware binaries.

The resulting commandlines are:
```
ESP_HOSTNAME=192.168.1.5 make wiflash
```
or assuming mDNS is working:
```
ESP_HOSTNAME=esp-link.local make wiflash
```
or using wiflash.sh:
```
./wiflash.sh <esp-hostname> user1.bin user2.bin
```

The flashing, restart, and re-associating with your wireless network takes about 15 seconds
and is fully automatic. The first 1MB of flash are divided into two 512KB partitions allowing for new
code to be uploaded into one partition while running from the other. This is the official
OTA upgrade method supported by the SDK, except that the firmware is POSTed to the module
using curl as opposed to having the module download it from a cloud server. On a module with
512KB flash there is only space for one partition and thus no way to do an OTA update.

If you need to clear the wifi settings you need to reflash the `blank.bin`
using the serial method.

The flash configuration and the OTA upgrade process is described in more detail
in [FLASH.md](FLASH.md).

## Installing esptool.py on Linux

On Linux use [esptool.py](https://github.com/themadinventor/esptool) to flash the esp8266.
If you're a little python challenged then the following install instructions might help:
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

## Installing esptool.py on Windows

Esptool is a pythin pgm that works just fine on windows. These instructions assume that git and
python are available from the commandline.

Start a command line, clone esptool, and run `python setup.py install` in esptool's
directory (this step needs to be done only once):
```
> git clone https://github.com/themadinventor/esptool.git
Cloning into 'esptool'...
remote: Counting objects: 268, done.
emote: Total 268 (delta 0), reused 0 (delta 0), pack-reused 268
Receiving objects: 100% (268/268), 99.66 KiB | 0 bytes/s, done.
Resolving deltas: 100% (142/142), done.
Checking connectivity... done.

> cd esptool

> python setup.py install
running install
...
...
...
Finished processing dependencies for esptool==0.1.0
```

Download and unzip the latest esp-link release package, and start a commandline
in that directory. The command to run is pretty much the same as for linux.
Adjust the path to esptool and the COM port if you don't have the ESP on COM12. 460800
baud worked just fine for me, writing at ~260kbit/s instead of ~80kbit/s.
```
>python "../esptool/esptool.py" --port COM12 --baud 115200 write_flash \
  --flash_freq 80m --flash_mode qio --flash_size 32m \
  0x0000 boot_v1.6.bin 0x1000 user1.bin \
  0x3FC000 esp_init_data_default.bin 0x3FE000 blank.bin
Connecting...
Erasing flash...
Wrote 3072 bytes at 0x00000000 in 0.3 seconds (79.8 kbit/s)...
Erasing flash...
Wrote 438272 bytes at 0x00001000 in 43.4 seconds (80.7 kbit/s)...
Erasing flash...
Wrote 1024 bytes at 0x003fc000 in 0.1 seconds (83.6 kbit/s)...
Erasing flash...
Wrote 4096 bytes at 0x003fe000 in 0.4 seconds (83.4 kbit/s)...

Leaving...
```
