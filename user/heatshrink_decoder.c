#include "httpdconfig.h"
#ifdef EFS_HEATSHRINK
//Stupid wrapper so we don't have to move c-files around
//Also loads httpd-specific config.

#define _STDLIB_H_
#define _STRING_H_
#define _STDDEF_H
#define _STDINT_H

#include "espmissingincludes.h"
#include "c_types.h"
#include "mem.h"
#include "osapi.h"
#include "heatshrink_config_httpd.h"
#define memset(x,y,z) os_memset(x,y,z)
#define memcpy(x,y,z) os_memcpy(x,y,z)
#include "../lib/heatshrink/heatshrink_decoder.c"

#endif
