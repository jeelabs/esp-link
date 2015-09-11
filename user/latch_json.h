#ifndef _LATCH_JSON_H_
#define _LATCH_JSON_H_
#include <esp8266.h>

typedef struct {
  uint8_t fallbackStateBits;
  uint8_t stateBits;
  uint8_t init;
  uint8_t fallbackSecondsForBits[8];
} LatchState;

#endif // _LATCH_JSON_H_