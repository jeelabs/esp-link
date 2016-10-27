#ifndef CGITELNET_H
#define CGITELNET_H

#include "httpd.h"

int cgiTelnetSet(HttpdConnData *connData);
int cgiTelnetInfo(HttpdConnData *connData);

#endif // CGITELNET_H
