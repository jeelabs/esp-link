#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
  uint32_t seq; // flash write sequence number
  uint16_t magic, crc;
  int8_t   reset_pin, isp_pin, conn_led_pin, ser_led_pin;
  int32_t  baud_rate;
  char     hostname[32];               // if using DHCP
  uint32_t staticip, netmask, gateway; // using DHCP if staticip==0
  uint8_t  log_mode;                   // UART log debug mode
} FlashConfig;
extern FlashConfig flashConfig;

bool configSave(void);
bool configRestore(void);
void configWipe(void);

#endif
