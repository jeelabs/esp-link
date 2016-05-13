#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <esp8266.h>

#include "httpd.h"
#include "cmd.h"

typedef enum
{
  LOAD=0,
  REFRESH,
  BUTTON,
  SUBMIT,

  INVALID=-1,
} RequestReason;

typedef enum
{
  WEB_STRING=0,
  WEB_NULL,
  WEB_INTEGER,
  WEB_BOOLEAN,
  WEB_FLOAT,
  WEB_JSON
} WebValueType;

void   WEB_Init();

char * WEB_UserPages();

int    WEB_CgiJsonHook(HttpdConnData *connData);
void   WEB_JsonData(CmdPacket *cmd);

#endif /* WEB_SERVER_H */

