#ifndef HTTPDESPFS_H
#define HTTPDESPFS_H

#include "httpd.h"

int cgiEspFsHook(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiEspFsTemplate(HttpdConnData *connData);
//int ICACHE_FLASH_ATTR cgiEspFsHtml(HttpdConnData *connData);

#endif
