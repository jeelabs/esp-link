#ifndef CONSOLE_H
#define CONSOLE_H

#include "httpd.h"

void consoleInit(void);
int tplConsole(HttpdConnData *connData, char *token, void **arg);

#endif
