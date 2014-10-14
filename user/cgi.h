#ifndef CGI_H
#define CGI_H

#include "httpd.h"

int cgiLed(HttpdConnData *connData);
int cgiReadFlash(HttpdConnData *connData);
int cgiTest(HttpdConnData *connData);
int cgiWiFiScan(HttpdConnData *connData);
void ICACHE_FLASH_ATTR tplWlan(HttpdConnData *connData, char *token, void **arg);
int ICACHE_FLASH_ATTR cgiWiFi(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiWiFiConnect(HttpdConnData *connData);

#endif