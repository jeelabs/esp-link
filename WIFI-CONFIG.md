Esp-link Wifi configuration
===========================

For proper operation the end state that esp-link needs to arrive at is to have it
join your pre-existing wifi network as a pure station.
However, in order to get there esp-link will start out as an access point and you'll have
to join its network to configure it. The short version is:

 1. esp-link creates a wifi access point with an SSID of the form `ESP_012ABC` (some modules
    use a different SSID form, such as `ai-thinker-012ABC`)
 2. you join your laptop or phone to esp-link's network as a station and you configure
    esp-link wifi with your network info by pointing your browser at `http://192.168.4.1/`
 3. you set a hostname for esp-link on the "home" page, or leave the default ("esp-link")
 4. esp-link starts to connect to your network while continuing to also be an access point
    ("AP+STA"), the esp-link may show up with a `${hostname}.local` hostname
    (depends on your DHCP/DNS config)
 4. esp-link succeeds in connecting and shuts down its own access point after 15 seconds,
    you reconnect your laptop/phone to your normal network and access esp-link via its hostname
    or IP address

### Notes on using AP (access point) mode

Esp-link does not support STA+AP mode, however it does support STA mode and AP mode. What happens
is that STA+AP mode is used at boot and when making STA changes to allow for recovery: the AP
mode stays on for a while so you can connect to it and fix the STA mode. Once STA has connected,
esp-link switches to STA-only mode. There is no setting to stay in STA+AP mode. So... if you want
to use AP ensure you set esp-link to AP-only mode. If you want STA+AP mode you're gonna have to
modify the source for yourself. (This stuff is painful to test and rather tricky, so don't expect
the way it works to change.)

Configuration details
---------------------

### Wifi

After you have serially flashed the module it will create a wifi access point (AP) with an
SSID of the form `ESP_012ABC` where 012ABC is a piece of the module's MAC address.
Using a laptop, phone, or tablet connect to this SSID and then open a browser pointed at
http://192.168.4.1/, you should then see the esp-link web site.

Now configure the wifi. The desired configuration is for the esp-link to be a
station on your local wifi network so you can communicate with it from all your computers.

To make this happen, navigate to the wifi page and you should see the esp-link scan
for available networks. You should then see a list of detected networks on the web page and you
can select yours.
Enter a password if your network is secure (highly recommended...) and hit the connect button.

You should now see that the esp-link has connected to your network and it should show you
its IP address. _Write it down_. You will then have to switch your laptop, phone, or tablet
back to your network and then you can connect to the esp-link's IP address or, depending on your
network's DHCP/DNS config you may be able to go to http://esp-link.local

At this point the esp-link will have switched to STA mode and be just a station on your
wifi network. These settings are stored in flash and thereby remembered through resets and
power cycles. They are also remembered when you flash new firmware. Only flashing `blank.bin`
via the serial port as indicated above will reset the wifi settings.

There is a fail-safe, which is that after a reset or a configuration change, if the esp-link
cannot connect to your network it will revert back to AP+STA mode after 15 seconds and thus
both present its `ESP_012ABC`-style network and continue trying to reconnect to the requested network.
You can then connect to the esp-link's AP and reconfigure the station part.

One open issue (#28) is that esp-link cannot always display the IP address it is getting to the browser
used to configure the ssid/password info. The problem is that the initial STA+AP mode may use
channel 1 and you configure it to connect to an AP on channel 6. This requires the ESP8266's AP
to also switch to channel 6 disconnecting you in the meantime. 

### Hostname, description, DHCP, mDNS

You can set a hostname on the "home" page, this should be just the hostname and not a domain
name, i.e., something like "test-module-1" and not "test-module-1.mydomain.com".
This has a number of effects:

- you will see the first 12 chars of the hostname in the menu bar (top left of the page) so
  if you have multiple modules you can distinguish them visually
- esp-link will use the hostname in its DHCP request, which allows you to identify the module's
  MAC and IP addresses in your DHCP server (typ. your wifi router). In addition, some DHCP
  servers will inject these names into the local DNS cache so you can use URLs like
  `hostname.local`.
- someday, esp-link will inject the hostname into mDNS (multicast DNS, bonjour, etc...) so 
  URLs of the form `hostname.local` work for everyone (as of v2.1.beta5 mDNS is disabled due
  to reliability issues with it)

You can also enter a description of up to 128 characters on the home page (bottom right). This
allows you to leave a memo for yourself, such as "installed in basement to control the heating
system". This descritpion is not used anywhere else.
