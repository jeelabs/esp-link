#include "EspLink.h"
#include "WebServer.h"

#define LED_PIN 13

void simpleLedHtmlCallback(WebServerCommand command, char * data, int dataLen);
const char simpleLedURL[] PROGMEM = "/SimpleLED.html.json";

const WebMethod PROGMEM methods[] = {
  { simpleLedURL, simpleLedHtmlCallback },
  { NULL, NULL },
};

WebServer webServer(Serial, methods);

void simpleLedHtmlCallback(WebServerCommand command, char * data, int dataLen)
{
  switch(command)
  {
    case BUTTON_PRESS:
      if( strcmp_P(data, PSTR("btn_on") ) == 0 )
        digitalWrite(LED_PIN, true);
      else if( strcmp_P(data, PSTR("btn_off") ) == 0 )
        digitalWrite(LED_PIN, false);
      break;
    case SET_FIELD:
      // no fields to set
      break;
    case LOAD:
    case REFRESH:
      if( digitalRead(LED_PIN) )
        webServer.setArgString("text", "LED is on");
      else
        webServer.setArgString("text", "LED is off");
      break;
  }
}

void setup()
{
  Serial.begin(57600);
  webServer.init();
}

void loop()
{
  webServer.loop();
}

