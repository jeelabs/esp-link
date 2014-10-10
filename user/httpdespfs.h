#ifndef HTTPDESPFS_H
#define HTTPDESPFS_H

#include "httpd.h"
#include "espfs.h"

int cgiEspFsHook(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiEspFsTemplate(HttpdConnData *connData);

#endif