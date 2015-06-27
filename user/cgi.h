#ifndef CGI_H
#define CGI_H

#include "httpd.h"

void jsonHeader(HttpdConnData *connData, int code);
int cgiMenu(HttpdConnData *connData);

#endif
