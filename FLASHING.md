Flashing esp-link
=================

### Hardware configuration

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
- GPIO13: connect to ISP of LPC/ARM microcontroller or to GPIO0 of esp8266 being programmed
  (not used with Arduino/AVR)
- GPIO0: optionally connect green "conn" LED to 3.3V (indicates wifi status)
- GPIO2: optionally connect yellow "ser" LED to 3.3V (indicates serial activity)

If your application has problems with the boot message that is output at ~74600 baud by the ROM
at boot time you can connect an esp-12 module as follows and choose the "swap_uart" pin assignment
in the esp-link web interface:

- GPIO13: connect to TX of microcontroller
- GPIO15: connect to RX of microcontroller
- GPIO1/UTXD: connect to RESET of microcontroller
- GPIO3/URXD: connect to ISP of LPC/ARM microcontroller or to GPIO0 of esp8266 being programmed
  (not used with Arduino/AVR)
- GPIO0: optionally connect green "conn" LED to 3.3V (indicates wifi status)
- GPIO2: optionally connect yellow "ser" LED to 3.3V (indicates serial activity)

If you are using an FTDI connector, GPIO12 goes to DTR and GPIO13 goes to CTS (or vice-versa, I've
seen both used, sigh).

The GPIO pin assignments can be changed dynamically in the web UI and are saved in flash.

### Initial serial flashing

If you want to simply flash a pre-built firmware binary, you can download the latest
[release](https://github.com/jeelabs/esp-link/releases) and use your favorite
ESP8266 flashing tool to flash the bootloader, the firmware, and blank settings.
Detailed instructions are provided in the release notes.

_Important_: the firmware adapts automatically to the size of the flash chip using information
stored in the boot sector (address 0). This is the standard way that the esp8266 SDK detects
the flash size. What this means is that you need to set this properly when you flash the bootloader.
If you use esptool.py you can do it using the -ff and -fs options.
