Esp-link: Outbound HTTP REST requests and MQTT client
-------------------------------------------

The V2 versions of esp-link use the SLIP protocol over the serial link to support simple outbound
HTTP REST requests as well as an MQTT client. The SLIP protocol consists of commands with
binary arguments sent from the
attached microcontroller to the esp8266, which then performs the command and responds back.
The responses back use a callback address in the attached microcontroller code, i.e., the
command sent by the uC contains a callback address and the response from the esp8266 starts
with that callback address. This enables asynchronous communication where esp-link can notify the
uC when requests complete or when other actions happen, such as wifi connectivity status changes.

You can find REST and MQTT libraries as well as demo sketches in the
[el-client](https://github.com/jeelabs/el-client) repository.
