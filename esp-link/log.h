#ifndef LOG_H
#define LOG_H

#include "httpd.h"

#define LOG_MODE_AUTO 0
#define LOG_MODE_OFF  1
#define LOG_MODE_ON   2

void logInit(void);
void log_uart(bool enable);
int ajaxLog(HttpdConnData *connData);
int ajaxLogDbg(HttpdConnData *connData);

#endif
