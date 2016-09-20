ESP-LINK web-server tutorial
============================

LED flashing sample
--------------------

Circuit:

. connect a Nodemcu (ESP8266) board and an Arduino Nano / UNO:
   (RX - levelshifter - TX, TX - levelshifter - RX)
. optionally connect RESET-s with a level shifter


Installation steps:

. install the latest Arduino on the PC
. install EspLink library from arduino/libraries path
. open EspLinkWebSimpleLedControl sample from Arduino
. upload the code onto an Arduino Nano/Uno
. install esp-link
. jump to the Web Server page on esp-link UI
. upload SimpleLED.html ( arduino/libraries/EspLink/examples/EspLinkWebSimpleLedControl/SimpleLED.html )
. jump to SimpleLED page on esp-link UI
. turn on/off the LED

Complex application sample
--------------------------

Circuit:

. connect a Nodemcu (ESP8266) board and an Arduino Nano / UNO:
   (RX - levelshifter - TX, TX - levelshifter - RX)
. optionally connect RESET-s with a level shifter
. add a trimmer to A0 for Voltage measurement

Installation steps:

. open EspLinkWebApp sample from Arduino
. upload the code onto an Arduino Nano/Uno
. jump to the Web Server page on esp-link UI
. upload web-page.espfs.img (  arduino/libraries/EspLink/examples/EspLinkWebApp/web-page.espfs.img )
. jump to LED/User/Voltage pages
. try out different settings
