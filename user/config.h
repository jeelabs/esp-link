#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
  int8_t  reset_pin, isp_pin, conn_led_pin, ser_led_pin;
  int32_t baud_rate;
  char    hostname[32];
} FlashConfig;
extern FlashConfig flashConfig;

bool configSave(void);
bool configRestore(void);

#endif
