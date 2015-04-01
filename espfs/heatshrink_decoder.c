#include "espfs.h"
#ifdef ESPFS_HEATSHRINK
//Stupid wrapper so we don't have to move c-files around
//Also loads httpd-specific config.

#ifdef __ets__
//esp build

#include <esp8266.h>

#define memset(x,y,z) os_memset(x,y,z)
#define memcpy(x,y,z) os_memcpy(x,y,z)
#endif

#include "heatshrink_config_custom.h"
#include "../lib/heatshrink/heatshrink_decoder.c"


#endif
