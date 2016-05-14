
#include "WebServer.h"

#define LED_PIN  13

int8_t  blinking = 0;
int8_t  frequency = 10;
uint8_t pattern = 2;
uint16_t elapse = 100;
uint16_t elapse_delta = 200;
uint32_t next_ts = 0;

void ledHtmlCallback(WebServerCommand command, char * data, int dataLen);

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

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, false);
}

void loop()
{
  webServer.loop();
  
  if( blinking )
  {
    if( next_ts <= millis() )
    {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      next_ts += elapse;
      elapse = elapse_delta - elapse;
    }
  }
}


void ledHtmlCallback(WebServerCommand command, char * data, int dataLen)
{
  switch(command)
  {
    case BUTTON_PRESS:
      if( strcmp_P(data, PSTR("btn_on") ) == 0 )
      {
        blinking = 0;
        digitalWrite(LED_PIN, true);
      } else if( strcmp_P(data, PSTR("btn_off") ) == 0 )
      {
        blinking = 0;
        digitalWrite(LED_PIN, false);
      } else if( strcmp_P(data, PSTR("btn_blink") ) == 0 )
      {
        blinking = 1;
        next_ts = millis() + elapse;
      }
      break;
    case SET_FIELD:
      if( strcmp_P(data, PSTR("frequency") ) == 0 )
      {
        frequency = webServer.getArgInt();
        digitalWrite(LED_PIN, false);
        elapse_delta = 2000 / frequency;
        elapse = pattern * elapse_delta / 4;
      }
      else if( strcmp_P(data, PSTR("pattern") ) == 0 )
      {
        char * arg = webServer.getArgString();

        if( strcmp_P(arg, PSTR("25_75")) == 0 )
          pattern = 1;
        else if( strcmp_P(arg, PSTR("50_50")) == 0 )
          pattern = 2;
        else if( strcmp_P(arg, PSTR("75_25")) == 0 )
          pattern = 3;

        digitalWrite(LED_PIN, false);
        elapse = pattern * elapse_delta / 4;
      }
      break;
    case LOAD:
      webServer.setArgNum(3);
      webServer.setArgInt("frequency", frequency);

      switch(pattern)
      {
        case 1:
          webServer.setArgStringP("pattern", PSTR("25_75"));
          break;
        case 2:
          webServer.setArgStringP("pattern", PSTR("50_50"));
          break;
        case 3:
          webServer.setArgStringP("pattern", PSTR("75_25"));
          break;
      }
    case REFRESH:
      if( command == REFRESH )
        webServer.setArgNum(1);

      if( blinking )
        webServer.setArgStringP("text", PSTR("LED is blinking"));
      else
        webServer.setArgStringP("text", digitalRead(LED_PIN) ? PSTR("LED is turned on") : PSTR("LED is turned off"));
      break;
    default:
      break;
  }
}


