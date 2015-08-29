// Combined include file for esp8266
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
extern char* esp_link_version;

void ICACHE_FLASH_ATTR init(void);

#ifdef __WIN32__
#include <_mingw.h>
#endif
