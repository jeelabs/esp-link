// Copyright (c) 2015 by Thorsten von Eicken, see LICENSE.txt in the esp-link repo

#ifndef OPTIBOOT_H
#define OPTIBOOT_H

#include <httpd.h>

int ICACHE_FLASH_ATTR cgiOptibootSync(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiOptibootData(HttpdConnData *connData);

#endif
