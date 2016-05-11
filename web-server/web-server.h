#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <esp8266.h>

#include "httpd.h"

typedef enum
{
  LOAD=0,
  REFRESH,
  BUTTON,
  SUBMIT,

  INVALID=-1,
} RequestReason;

void   webServerInit();

char * webServerUserPages();

int    ICACHE_FLASH_ATTR cgiJsonHook(HttpdConnData *connData);

#endif /* WEB_SERVER_H */

