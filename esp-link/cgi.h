#ifndef CGI_H
#define CGI_H

#include <esp8266.h>
#include "httpd.h"

void jsonHeader(HttpdConnData *connData, int code);
int cgiMenu(HttpdConnData *connData);
uint8_t UTILS_StrToIP(const char* str, void *ip);

#endif
