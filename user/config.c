/* Configuration stored in flash */

#include <esp8266.h>
#include <osapi.h>
#include "config.h"
#include "espfs.h"

FlashConfig flashConfig;
FlashConfig flashDefault = {
  33, 0,
  MCU_RESET_PIN, MCU_ISP_PIN, LED_CONN_PIN, LED_SERIAL_PIN,
  115200,
  "esp-link\0                       ",
};

#define FLASH_ADDR   (0x3E000)
#define FLASH_SECT   (4096)
static int flash_pri; // primary flash sector (0 or 1, or -1 for error)

bool ICACHE_FLASH_ATTR configSave(void) {
  FlashConfig fc;
  memcpy(&fc, &flashConfig, sizeof(FlashConfig));
  uint32_t seq = fc.seq+1;
  // erase secondary
  uint32_t addr = FLASH_ADDR + (1-flash_pri)*FLASH_SECT;
  if (spi_flash_erase_sector(addr>>12) != SPI_FLASH_RESULT_OK)
    return false; // no harm done, give up
  // write primary with incorrect seq
  fc.seq = 0xffffffff;
  fc.crc = 0x55aa55aa;
  if (spi_flash_write(addr, (void *)&fc, sizeof(FlashConfig)) != SPI_FLASH_RESULT_OK)
    return false; // no harm done, give up
  // fill in correct seq
  fc.seq = seq;
  if (spi_flash_write(addr, (void *)&fc, sizeof(uint32_t)) != SPI_FLASH_RESULT_OK)
    return false; // most likely failed, but no harm if successful
  // now that we have safely written the new version, erase old primary
  addr = FLASH_ADDR + flash_pri*FLASH_SECT;
  flash_pri = 1-flash_pri;
  if (spi_flash_erase_sector(addr>>12) != SPI_FLASH_RESULT_OK)
    return true; // no back-up but we're OK
  // write secondary
  fc.seq = 0xffffffff;
  if (spi_flash_write(addr, (void *)&fc, sizeof(FlashConfig)) != SPI_FLASH_RESULT_OK)
    return true; // no back-up but we're OK
  fc.seq = seq;
  spi_flash_write(addr, (void *)&fc, sizeof(uint32_t));
  return true;
}

void ICACHE_FLASH_ATTR configWipe(void) {
  spi_flash_erase_sector(FLASH_ADDR>>12);
  spi_flash_erase_sector((FLASH_ADDR+FLASH_SECT)>>12);
}

static uint32_t ICACHE_FLASH_ATTR selectPrimary(FlashConfig *fc0, FlashConfig *fc1);

bool ICACHE_FLASH_ATTR configRestore(void) {
  FlashConfig fc0, fc1;
  // read both flash sectors
  if (spi_flash_read(FLASH_ADDR, (void *)&fc0, sizeof(FlashConfig)) != SPI_FLASH_RESULT_OK)
    memset(&fc0, sizeof(FlashConfig), 0); // clear in case of error
  if (spi_flash_read(FLASH_ADDR+FLASH_SECT, (void *)&fc1, sizeof(FlashConfig)) != SPI_FLASH_RESULT_OK)
    memset(&fc1, sizeof(FlashConfig), 0); // clear in case of error
  // figure out which one is good
  flash_pri = selectPrimary(&fc0, &fc1);
  // if neither is OK, we revert to defaults
  if (flash_pri < 0) {
    memcpy(&flashConfig, &flashDefault, sizeof(FlashConfig));
    flash_pri = 0;
    return false;
  }
  // copy good one into global var and return
  memcpy(&flashConfig, flash_pri == 0 ? &fc0 : &fc1, sizeof(FlashConfig));
  return true;
}

static uint32_t ICACHE_FLASH_ATTR selectPrimary(FlashConfig *fc0, FlashConfig *fc1) {
  bool fc0_crc_ok = fc0->crc == 0x55aa55aa && fc0->seq != 0xffffffff; // need real crc checksum...
  bool fc1_crc_ok = fc1->crc == 0x55aa55aa && fc1->seq != 0xffffffff; // need real crc checksum...
  if (fc0_crc_ok)
    if (!fc1_crc_ok || fc0->seq >= fc1->seq)
      return 0; // use first sector as primary
    else
      return 1; // second sector is newer
  else
    return fc1_crc_ok ? 1 : -1;
}
