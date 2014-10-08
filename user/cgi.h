#ifndef CGI_H
#define CGI_H

#include "httpd.h"

int cgiLed(HttpdConnData *connData);
int cgiTest(HttpdConnData *connData);

#endif