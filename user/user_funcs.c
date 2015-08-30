#include "user_funcs.h"

bool ICACHE_FLASH_ATTR pwmPinStateForSchedule(uint8_t onHour, uint8_t onMinute, uint8_t offHour, uint8_t offMinute) {
  uint16_t NumMinsToday = totalMinutes(hour(), minute());
  bool state = false;

  if (totalMinutes(offHour, offMinute) > totalMinutes(onHour, onMinute)) {
    state = (NumMinsToday >= totalMinutes(onHour, onMinute)) ? true : false;

    if (NumMinsToday >= totalMinutes(offHour, offMinute))
      state = false;
  }
  else {
    state = (NumMinsToday >= totalMinutes(offHour, offMinute)) ? false : true;

    if (NumMinsToday >= totalMinutes(onHour, onMinute))
      state = true;
  }
  return state;
}

const char* ICACHE_FLASH_ATTR byteToBin(uint8_t num) {
  static char b[9];
  b[0] = '\0';

  int z;
  for (z = 128; z > 0; z >>= 1) {
    strcat(b, ((num & z) == z) ? "1" : "0");
  }
  return b;
}

const uint8_t ICACHE_FLASH_ATTR binToByte(char* bin_str) {
  char * tmp;
  long x = strtol(bin_str, &tmp, 2);
  return (x <= 255) ? (uint8_t)x : -1;
}
