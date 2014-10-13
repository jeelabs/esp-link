#include "httpdconfig.h"
#ifdef EFS_HEATSHRINK
//Stupid wrapper so we don't have to move c-files around
//Also loads httpd-specific config.

#include "../lib/heatshrink/heatshrink_decoder.c"

#endif
