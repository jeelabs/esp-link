ESP-LINK web-server tutorial
============================

LED flashing sample
--------------------

Circuit:

 - 1: connect a Nodemcu (ESP8266) board and an Arduino Nano / UNO:
   (RX - levelshifter - TX, TX - levelshifter - RX)
 - 2: optionally connect RESET-s with a level shifter


Installation steps:

 - 1: install the latest Arduino on the PC
 - 2: install EspLink library from arduino/libraries path
 - 3: open EspLinkWebSimpleLedControl sample from Arduino
 - 4: upload the code onto an Arduino Nano/Uno
 - 5: install esp-link
 - 6: jump to the Web Server page on esp-link UI
 - 7: upload SimpleLED.html ( arduino/libraries/EspLink/examples/EspLinkWebSimpleLedControl/SimpleLED.html )
 - 8: jump to SimpleLED page on esp-link UI
 - 9: turn on/off the LED

Complex application sample
--------------------------

Circuit:

 - 1: connect a Nodemcu (ESP8266) board and an Arduino Nano / UNO:
   (RX - levelshifter - TX, TX - levelshifter - RX)
 - 2: optionally connect RESET-s with a level shifter
 - 3: add a trimmer to A0 for Voltage measurement

Installation steps:

 - 1: open EspLinkWebApp sample from Arduino
 - 2: upload the code onto an Arduino Nano/Uno
 - 3: jump to the Web Server page on esp-link UI
 - 4: upload web-page.espfs.img (  arduino/libraries/EspLink/examples/EspLinkWebApp/web-page.espfs.img )
 - 5: jump to LED/User/Voltage pages
 - 6: try out different settings
 
