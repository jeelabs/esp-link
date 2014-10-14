
//Including the system-wide stdint.h messes stuff up... but I don't want to change heatshrink
//not to do it. Including this dummy file fixes it too, tho'.

#ifndef __ets__
//Do include stdint for testing builds.
#include_next <stdint.h>
#endif