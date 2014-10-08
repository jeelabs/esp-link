#ifndef HTTPDESPFS_H
#define HTTPDESPFS_H

#include "httpd.h"
#include "espfs.h"

int cgiEspFsHook(HttpdConnData *connData);

#endif