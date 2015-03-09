#ifndef CGI_H
#define CGI_H

#include "httpd.h"

int cgiLed(HttpdConnData *connData);
int tplLed(HttpdConnData *connData, char *token, void **arg);
int cgiReadFlash(HttpdConnData *connData);
int tplCounter(HttpdConnData *connData, char *token, void **arg);
int cgiUploadEspfs(HttpdConnData *connData);

#endif