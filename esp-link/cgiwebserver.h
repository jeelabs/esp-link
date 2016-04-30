#ifndef CGIWEBSERVER_H
#define CGIWEBSERVER_H

#include <httpd.h>

int ICACHE_FLASH_ATTR cgiWebServerUpload(HttpdConnData *connData);

#endif /* CGIWEBSERVER_H */
