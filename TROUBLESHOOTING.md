Esp-Link troubleshooting
========================

### Troubleshooting

- verify that you have sufficient power, borderline power can cause the esp module to seemingly
  function until it tries to transmit and the power rail collapses
- if you just cannot flash your esp8266 module (some people call it the zombie mode) make sure you
  have gpio0 and gpio15 pulled to gnd with a 1K resistor, gpio2 tied to 3.3V with 1K resistor, and
  RX/TX connected without anything in series. If you need to level shift the signal going into the
  esp8266's RX use a 1K resistor. Use 115200 baud in the flasher.
  (For a permanent set-up I would use higher resistor values but
  when nothing seems to work these are the ones I try.)
- if the flashing succeeded, check the "conn" LED to see which mode esp-link is in (see LED info above)
- reset or power-cycle the esp-link to force it to become an access-point if it can't
  connect to your network within 15-20 seconds
- if the LED says that esp-link is on your network but you can't get to it, make sure your
  laptop is on the same network (and no longer on the esp's network)
- if you do not know the esp-link's IP address on your network, try `esp-link.local`, try to find
  the lease in your DHCP server; if all fails, you may have to turn off your access point (or walk
  far enough away) and reset/power-cycle esp-link, it will then fail to connect and start its
  own AP after 15-20 seconds

### LED indicators

Assuming appropriate hardware attached to GPIO pins, the green "conn" LED will show the wifi
status as follows:

- Very short flash once a second: not connected to a network and running as AP+STA, i.e.
  trying to connect to the configured network
- Very short flash once every two seconds: not connected to a network and running as AP-only
- Even on/off at 1HZ: connected to the configured network but no IP address (waiting on DHCP)
- Steady on with very short off every 3 seconds: connected to the configured network with an
  IP address (esp-link shuts down its AP after 60 seconds)

The yellow "ser" LED will blink briefly every time serial data is sent or received by the esp-link.
