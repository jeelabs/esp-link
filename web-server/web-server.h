#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <esp8266.h>

#include "httpd.h"

void   webServerInit();

char * webServerUserPages();

int    webServerProcessJsonQuery(HttpdConnData *connData);

#endif /* WEB_SERVER_H */

