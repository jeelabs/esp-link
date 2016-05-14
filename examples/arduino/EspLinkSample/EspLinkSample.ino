
#include "WebServer.h"
#include "Pages.h"

const char ledURL[] PROGMEM = "/LED.html.json";

const WebMethod PROGMEM methods[] = {
  { ledURL, ledHtmlCallback },
  { NULL, NULL },
};

WebServer webServer(Serial, methods);

void setup()
{
  Serial.begin(57600);
  webServer.init();

  ledInit();
}

void loop()
{
  webServer.loop();

  ledLoop();
}

