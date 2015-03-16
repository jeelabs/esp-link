#ifndef CGIFLASH_H
#define CGIFLASH_H

#include "httpd.h"

int cgiReadFlash(HttpdConnData *connData);
int cgiUploadEspfs(HttpdConnData *connData);

#endif