#include "WebServer.h"
#include "Pages.h"

const char ledURL[] PROGMEM = "/LED.html.json";
const char userURL[] PROGMEM = "/User.html.json";
const char voltageURL[] PROGMEM = "/Voltage.html.json";

const WebMethod PROGMEM methods[] = {
  { ledURL, ledHtmlCallback },
  { userURL, userHtmlCallback },
  { voltageURL, voltageHtmlCallback },
  { NULL, NULL },
};

WebServer webServer(Serial, methods);

void setup()
{
  Serial.begin(57600);
  webServer.init();

  ledInit();
  userInit();
  voltageInit();
}

void loop()
{
  webServer.loop();

  ledLoop();
  voltageLoop();
}

