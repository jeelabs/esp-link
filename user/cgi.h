#ifndef CGI_H
#define CGI_H

#include "httpd.h"

void ICACHE_FLASH_ATTR jsonHeader(HttpdConnData *connData, int code);
void ICACHE_FLASH_ATTR printGlobalJSON(HttpdConnData *connData);

#endif
