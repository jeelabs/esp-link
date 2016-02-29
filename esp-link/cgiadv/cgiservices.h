#ifndef CGISERVICES_H
#define CGISERVICES_H

#include "httpd.h"

int cgiSystemSet(HttpdConnData *connData);
int cgiSystemInfo(HttpdConnData *connData);

void cgiServicesSNTPInit();
int cgiServicesInfo(HttpdConnData *connData);
int cgiServicesSet(HttpdConnData *connData);

#endif // CGISERVICES_H
