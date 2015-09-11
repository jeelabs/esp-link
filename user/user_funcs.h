#ifndef _USER_FUNCS_H_
#define _USER_FUNCS_H_
#include <esp8266.h>
#include <Time.h>

bool pwmPinStateForSchedule(uint8_t onHour, uint8_t onMinute, uint8_t offHour, uint8_t offMinute);
const char* byteToBin(uint8_t num);
const uint8_t binToByte(char* bin_str);


#endif // _USER_FUNCS_H_