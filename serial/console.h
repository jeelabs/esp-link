#ifndef CONSOLE_H
#define CONSOLE_H

#include "httpd.h"

void consoleInit(void);
void ICACHE_FLASH_ATTR console_write_char(char c);
int ajaxConsole(HttpdConnData *connData);
int ajaxConsoleReset(HttpdConnData *connData);
int ajaxConsoleClear(HttpdConnData *connData);
int ajaxConsoleBaud(HttpdConnData *connData);
int ajaxConsoleFormat(HttpdConnData *connData);
int ajaxConsoleSend(HttpdConnData *connData);
int tplConsole(HttpdConnData *connData, char *token, void **arg);

#endif
