#include <esp8266.h>
#include "config.h"
#include "syslog.h"

#define APPINIT_DBG
#ifdef APPINIT_DBG
#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)
#else
#define DBG(format, ...) do { } while(0)
#endif

// initialize the custom stuff that goes beyond esp-link
void app_init() {

}
