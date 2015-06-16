#ifndef CGIPINS_H
#define CGIPINS_H

#include "httpd.h"

int cgiPins(HttpdConnData *connData);
int8_t pin_reset, pin_isp, pin_conn, pin_ser;

#endif
