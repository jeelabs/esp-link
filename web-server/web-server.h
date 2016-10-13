#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <esp8266.h>

#include "httpd.h"
#include "cmd.h"

typedef enum
{
  LOAD=0,        // loading web-page content at the first time
  REFRESH,       // loading web-page subsequently
  BUTTON,        // HTML button pressed
  SUBMIT,        // HTML form is submitted

  INVALID=-1,
} RequestReason;

typedef enum
{
  WEB_STRING=0,  // the value is string
  WEB_NULL,      // the value is NULL
  WEB_INTEGER,   // the value is integer
  WEB_BOOLEAN,   // the value is boolean
  WEB_FLOAT,     // the value is float
  WEB_JSON       // the value is JSON data
} WebValueType;

void   WEB_Init();

char * WEB_UserPages();

int    WEB_CgiJsonHook(HttpdConnData *connData);
void   WEB_Setup(CmdPacket *cmd);
void   WEB_Data(CmdPacket *cmd);

#endif /* WEB_SERVER_H */

