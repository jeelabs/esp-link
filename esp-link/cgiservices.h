#ifndef CGISERVICES_H
#define CGISERVICES_H

#include "httpd.h"

// Need to be in sync with html/services.html
#define	DST_NONE	0
#define	DST_EUROPE	1
#define	DST_USA		2

int cgiSystemSet(HttpdConnData *connData);
int cgiSystemInfo(HttpdConnData *connData);

void cgiServicesSNTPInit();
void cgiServicesCheckDST();

int cgiServicesInfo(HttpdConnData *connData);
int cgiServicesSet(HttpdConnData *connData);

extern char* rst_codes[7];
extern char* flash_maps[7];

#endif // CGISERVICES_H
