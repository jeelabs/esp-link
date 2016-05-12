
#include "WebServer.h"
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
    case SUBMIT:
      {
        uint16_t len = espLink.cmdArgLen(req);
        char bf[len+1];
        bf[len] = 0;
        espLink.cmdPopArg(req, bf, len);
        Serial.print("Arg: ");
        Serial.println(bf);

        if( reason == SUBMIT )
          return;
      }
      break;
  }

  char * json = "{\"last_name\": \"helloka\"}";

  espLink.sendPacketStart(CMD_WEB_JSON_DATA, 100, 3);
  espLink.sendPacketArg(4, ip);
  espLink.sendPacketArg(2, (uint8_t *)&port);
  espLink.sendPacketArg(strlen(json), (uint8_t *)json);
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

