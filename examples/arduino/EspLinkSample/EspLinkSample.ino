
#include "WebServer.h"

#define LED_PIN  13

int8_t blinking = 0;
int8_t frequency = 10;
uint16_t elapse = 100;
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
    case LOAD:
      webServer.setArgNum(1);
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


/*
#include "EspLink.h"


void packetReceived(CmdRequest *req);

EspLink espLink(Serial, packetReceived);

void packetReceived(CmdRequest *req)
{
  Serial.println("\nReceived\n");
  uint16_t shrt, port;
  espLink.cmdPopArg(req, &shrt, 2);
  RequestReason reason = (RequestReason)shrt;
  Serial.print("Reason: ");
  Serial.println(reason);

  uint8_t ip[4];
  espLink.cmdPopArg(req, &ip, 4);
  Serial.print("IP: ");
  for(int i=0; i < 4; i++)
  {
    Serial.print(ip[i], DEC);
    if( i != 3 )
      Serial.print(".");
  }
  Serial.println();
  
  espLink.cmdPopArg(req, &port, 2);
  Serial.print("Port: ");
  Serial.println(port);

  {
    uint16_t len = espLink.cmdArgLen(req);
    char bf[len+1];
    bf[len] = 0;
    espLink.cmdPopArg(req, bf, len);
    Serial.print("Url: ");
    Serial.println(bf);
  }

  switch( reason )
  {
    case BUTTON:
      {
        uint16_t len = espLink.cmdArgLen(req);
        char bf[len+1];
        bf[len] = 0;
        espLink.cmdPopArg(req, bf, len);
        Serial.print("Arg: ");
        Serial.println(bf);
      }
      break;
    case SUBMIT:
      {
          int arg = 4;
          while( espLink.cmdGetArgc(req) > arg )
          {
            arg++;

            uint16_t len = espLink.cmdArgLen(req);
            char bf[len+1];
            bf[len] = 0;
            espLink.cmdPopArg(req, bf, len);

            Serial.print(bf + 1);
            Serial.print(" -> ");
            Serial.println(bf + strlen(bf+1) + 2);
          }
          return;
      }
  }

  espLink.sendPacketStart(CMD_WEB_JSON_DATA, 100, 3);
  espLink.sendPacketArg(4, ip);
  espLink.sendPacketArg(2, (uint8_t *)&port);
  
  char outBuf[30];
  outBuf[0] = 0;
  strcpy(outBuf+1, "last_name");
  strcpy(outBuf+11,"helloka");
  espLink.sendPacketArg(19, (uint8_t *)outBuf);
  
  espLink.sendPacketEnd();
  
}

void setup() {
  Serial.begin(57600);

  delay(10);
  espLink.sendPacketStart(CMD_CB_ADD, 100, 1);
  espLink.sendPacketArg(5, (uint8_t *)"webCb");
  espLink.sendPacketEnd();
}

void loop() {
  espLink.readLoop();
}
*/
