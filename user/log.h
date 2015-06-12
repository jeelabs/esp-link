#ifndef LOG_H
#define LOG_H

#include "httpd.h"

void logInit(void);
void ICACHE_FLASH_ATTR log_uart(bool enable);
int ICACHE_FLASH_ATTR ajaxLog(HttpdConnData *connData);

#endif
