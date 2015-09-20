#ifndef CGI_H
#define CGI_H

#include <esp8266.h>
#include "httpd.h"

void jsonHeader(HttpdConnData *connData, int code);
void errorResponse(HttpdConnData *connData, int code, char *message);

// Get the HTTP query-string param 'name' and store it at 'config' with max length
// 'max_len' (incl terminating zero), returns -1 on error, 0 if not found, 1 if found
int8_t getStringArg(HttpdConnData *connData, char *name, char *config, int max_len);

// Get the HTTP query-string param 'name' and store it boolean value at 'config',
// supports 1/true and 0/false, returns -1 on error, 0 if not found, 1 if found
int8_t getBoolArg(HttpdConnData *connData, char *name, bool*config);

int cgiMenu(HttpdConnData *connData);

uint8_t UTILS_StrToIP(const char* str, void *ip);

#endif
