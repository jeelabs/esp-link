// Copyright (c) 2016-2017 by Danny Backx, see LICENSE.txt in the esp-link repo

#ifndef CGIMEGA_H
#define CGIMEGA_H

#include <httpd.h>

int ICACHE_FLASH_ATTR cgiMegaSync(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiMegaData(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiMegaRead(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiMegaFuse(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiMegaRebootMCU(HttpdConnData *connData);

#endif
