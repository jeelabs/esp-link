// Combined include file for esp8266
#ifndef _ESP8266_H_
#define _ESP8266_H_

#undef MEMLEAK_DEBUG
#define USE_OPTIMIZE_PRINTF

#define os_timer_arm_us(a,b,c) os_timer_arm(a,b/1000,c)

#include <user_config.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_types.h>
#include <ip_addr.h>
#include <espconn.h>
#include <ets_sys.h>
#include <gpio.h>
#include <mem.h>
#include <osapi.h>
#include <upgrade.h>

#include "espmissingincludes.h"
#include "uart_hw.h"

#ifdef __WIN32__
#include <_mingw.h>
#endif

#endif // _ESP8266_H_
