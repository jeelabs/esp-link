#ifndef LOG_H
#define LOG_H

#include "httpd.h"

#define LOG_MODE_AUTO 0  // start by logging to uart0, turn aff after we get an IP
#define LOG_MODE_OFF  1  // always off
#define LOG_MODE_ON0  2  // always log to uart0
#define LOG_MODE_ON1  3  // always log to uart1

void logInit(void);
void log_uart(bool enable);
int ajaxLog(HttpdConnData *connData);
int ajaxLogDbg(HttpdConnData *connData);

void dumpMem(void *addr, int len);

#endif
