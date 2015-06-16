/* Configuration stored in flash */

#include <esp8266.h>
#include <osapi.h>
#include "config.h"
#include "espfs.h"

FlashConfig flashConfig = {
  MCU_RESET_PIN, MCU_ISP_PIN, LED_CONN_PIN, LED_SERIAL_PIN,
  115200,
  "esp-link\0                       ",
};

bool ICACHE_FLASH_ATTR configSave(void) {
  return true;
}

bool ICACHE_FLASH_ATTR configRestore(void) {
  return true;
}
