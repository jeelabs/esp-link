#ifndef CGISERVICES_H
#define CGISERVICES_H

#include "httpd.h"

int cgiSystemSet(HttpdConnData *connData);
int cgiSystemInfo(HttpdConnData *connData);

void cgiServicesSNTPInit();
int cgiServicesInfo(HttpdConnData *connData);
int cgiServicesSet(HttpdConnData *connData);

extern char* rst_codes[7];
extern char* flash_maps[7];

#endif // CGISERVICES_H
