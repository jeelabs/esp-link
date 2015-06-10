#ifndef CGIFLASH_H
#define CGIFLASH_H

#include "httpd.h"

typedef struct {
	int espFsPos;
	int espFsSize;
} CgiUploadEspfsParams;


int cgiReadFlash(HttpdConnData *connData);
int cgiUploadEspfs(HttpdConnData *connData);

#endif