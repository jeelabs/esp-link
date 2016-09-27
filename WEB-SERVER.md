ESP-LINK web-server tutorial
============================

Video
--------------------

https://www.youtube.com/watch?v=vBESCO0UhYI


Installing el-client Arduino library
--------------------

Download and install ELClient library.

https://github.com/jeelabs/el-client


LED flashing sample
--------------------

Circuit:

 - 1: connect a Nodemcu (ESP8266) board and an Arduino Nano / UNO:
   (RX - levelshifter - TX, TX - levelshifter - RX)
 - 2: optionally connect RESET-s with a level shifter


Installation steps:

 - 1: open webserver_led ELClient sample file in Arduino
 - 2: upload the code onto an Arduino Nano/Uno
 - 3: open the Web Server page on esp-link UI
 - 4: upload LED.html from webserver_led ( ELCient/examples/webserver_led/LED.html )
 - 5: choose LED page on esp-link UI
 - 6: turn on/off the LED

 
HTML controls sample
--------------------------

Circuit:

 - 1: connect a Nodemcu (ESP8266) board and an Arduino Nano / UNO:
   (RX - levelshifter - TX, TX - levelshifter - RX)
 - 2: optionally connect RESET-s with a level shifter
 - 3: add a trimmer to A0 for voltage measurement

Installation steps:

 - 1: open webserver_controls ELClient sample file in Arduino
 - 2: upload the code onto an Arduino Nano/Uno
 - 3: open the Web Server page on esp-link UI
 - 4: upload the 3 HTML files from webserver_controls ( select multiple htmls from  ELCient/examples/webserver_controls/ )
 - 5: jump to LED/User/Voltage pages
 - 6: try out different settings

 
Supported HTML controls
--------------------

HTML&nbsp;control&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; | Value Type  | Description  | Form Submission |
 ------------------------ | ----- | ------------ | ------- |
&lt;p id="id"/&gt; <br/> &lt;div id="id"/&gt; <br/> &lt;tr id="id"/&gt; <br/> &lt;th id="id"/&gt; <br/> &lt;td id="id"/&gt; <br/> &lt;textarea id="id"/&gt; | String&nbsp;(HTML) | MCU can replace the inner HTML part of the control at LOAD/REFRESH queries. The string (sent by MCU) is handled as HTML, so &lt;img...&gt; will be displayed as an image on the page | NO |
&lt;button id="id"/&gt; | String | When button is pressed, a message is transmitted to MCU containing the id (BUTTON_PRESS) | NO |
&lt;input name="id"/&gt; | String <br/> Integer <br/> Float <br/> Boolean | MCU can replace the value or checked properties of the HTML control in the form (LOAD/REFRESH). At form submission, the content of value will be transmitted to MCU (SET_FIELD). | YES |
&lt;select name="id"/&gt; | String | MCU can choose a value from the drop down (LOAD/REFRESH). At form submission the currently selected value will be transmitted to MCU (SET_FIELD) | YES |
&lt;ul id="id"/&gt; <br/> &lt;ol id="id"/&gt; | JSON list <br/> ["1","2","3"] | MCU can send a JSON list which is transformed to an HTML list ( &lt;li/&gt; )  (LOAD/REFRESH) | NO |
&lt;table id="id"/&gt; | JSON table <br/> [["1","2"], <br/> ["3","4"]] | MCU sends a JSON table which is transformed to an HTML table  (LOAD/REFRESH) | NO |

