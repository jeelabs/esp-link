#ifndef CGI_H
#define CGI_H

#include <esp8266.h>
#include "httpd.h"

void noCacheHeaders(HttpdConnData *connData, int code);
void jsonHeader(HttpdConnData *connData, int code);
void errorResponse(HttpdConnData *connData, int code, char *message);

// Get the HTTP query-string param 'name' and store it at 'config' with max length
// 'max_len' (incl terminating zero), returns -1 on error, 0 if not found, 1 if found
int8_t getStringArg(HttpdConnData *connData, char *name, char *config, int max_len);

// Get the HTTP query-string param 'name' and store it as a int8_t value at 'config',
// supports signed and unsigned, returns -1 on error, 0 if not found, 1 if found
int8_t getInt8Arg(HttpdConnData *connData, char *name, int8_t *config);

// Get the HTTP query-string param 'name' and store it as a uint8_t value at 'config',
// supports signed and unsigned, returns -1 on error, 0 if not found, 1 if found
int8_t getUInt8Arg(HttpdConnData *connData, char *name, uint8_t *config);

// Get the HTTP query-string param 'name' and store it as a uint16_t value at 'config',
// supports signed and unsigned, returns -1 on error, 0 if not found, 1 if found
int8_t getUInt16Arg(HttpdConnData *connData, char *name, uint16_t *config);

// Get the HTTP query-string param 'name' and store it boolean value at 'config',
// supports 1/true and 0/false, returns -1 on error, 0 if not found, 1 if found
int8_t getBoolArg(HttpdConnData *connData, char *name, uint8_t *config);

int cgiMenu(HttpdConnData *connData);

uint8_t UTILS_StrToIP(const char *str, void *ip);

#endif
