#ifndef CGI_H
#define CGI_H

#include <esp8266.h>
#include "httpd.h"

void jsonHeader(HttpdConnData *connData, int code);
void errorResponse(HttpdConnData *connData, int code, char *message);
int getStringArg(HttpdConnData *connData, char *name, char *config, int max_len);
int getBoolArg(HttpdConnData *connData, char *name, bool*config);
int cgiMenu(HttpdConnData *connData);
uint8_t UTILS_StrToIP(const char* str, void *ip);
char* system_get_chip_id_str();

#endif
