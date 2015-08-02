// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt
/* Configuration stored in flash */

#include <esp8266.h>
#include <osapi.h>
#include "config.h"
#include "espfs.h"

// hack: this from LwIP
extern uint16_t inet_chksum(void *dataptr, uint16_t len);

FlashConfig flashConfig;
FlashConfig flashDefault = {
  33, 0, 0,
  MCU_RESET_PIN, MCU_ISP_PIN, LED_CONN_PIN, LED_SERIAL_PIN,
  115200,
  "esp-link\0                       ", // hostname
  0, 0x00ffffff, 0,                    // static ip, netmask, gateway
  0,                                   // log mode
  0,                                   // swap_uart
  1, 0,                                // tcp_enable, rssi_enable
  "\0",                                // api_key
};

typedef union {
  FlashConfig fc;
  uint8_t     block[128];
} FlashFull;

#define FLASH_MAGIC  (0xaa55)

#define FLASH_ADDR   (0x3E000)
#define FLASH_SECT   (4096)
static int flash_pri; // primary flash sector (0 or 1, or -1 for error)

#if 0
static void memDump(void *addr, int len) {
  for (int i=0; i<len; i++) {
    os_printf("0x%02x", ((uint8_t *)addr)[i]);
  }
  os_printf("\n");
}
#endif

bool ICACHE_FLASH_ATTR configSave(void) {
  FlashFull ff;
  memset(&ff, 0, sizeof(ff));
  memcpy(&ff, &flashConfig, sizeof(FlashConfig));
  uint32_t seq = ff.fc.seq+1;
  // erase secondary
  uint32_t addr = FLASH_ADDR + (1-flash_pri)*FLASH_SECT;
  if (spi_flash_erase_sector(addr>>12) != SPI_FLASH_RESULT_OK)
    goto fail; // no harm done, give up
  // calculate CRC
  ff.fc.seq = seq;
  ff.fc.magic = FLASH_MAGIC;
  ff.fc.crc = 0;
  //os_printf("cksum of: ");
  //memDump(&ff, sizeof(ff));
  ff.fc.crc = inet_chksum(&ff, sizeof(ff));
  //os_printf("cksum is %04x\n", ff.fc.crc);
  // write primary with incorrect seq
  ff.fc.seq = 0xffffffff;
  if (spi_flash_write(addr, (void *)&ff, sizeof(ff)) != SPI_FLASH_RESULT_OK)
    goto fail; // no harm done, give up
  // fill in correct seq
  ff.fc.seq = seq;
  if (spi_flash_write(addr, (void *)&ff, sizeof(uint32_t)) != SPI_FLASH_RESULT_OK)
    goto fail; // most likely failed, but no harm if successful
  // now that we have safely written the new version, erase old primary
  addr = FLASH_ADDR + flash_pri*FLASH_SECT;
  flash_pri = 1-flash_pri;
  if (spi_flash_erase_sector(addr>>12) != SPI_FLASH_RESULT_OK)
    return true; // no back-up but we're OK
  // write secondary
  ff.fc.seq = 0xffffffff;
  if (spi_flash_write(addr, (void *)&ff, sizeof(ff)) != SPI_FLASH_RESULT_OK)
    return true; // no back-up but we're OK
  ff.fc.seq = seq;
  spi_flash_write(addr, (void *)&ff, sizeof(uint32_t));
  return true;
fail:
  os_printf("*** Failed to save config ***\n");
  return false;
}

void ICACHE_FLASH_ATTR configWipe(void) {
  spi_flash_erase_sector(FLASH_ADDR>>12);
  spi_flash_erase_sector((FLASH_ADDR+FLASH_SECT)>>12);
}

static uint32_t ICACHE_FLASH_ATTR selectPrimary(FlashFull *fc0, FlashFull *fc1);

bool ICACHE_FLASH_ATTR configRestore(void) {
  FlashFull ff0, ff1;
  // read both flash sectors
  if (spi_flash_read(FLASH_ADDR, (void *)&ff0, sizeof(ff0)) != SPI_FLASH_RESULT_OK)
    memset(&ff0, 0, sizeof(ff0)); // clear in case of error
  if (spi_flash_read(FLASH_ADDR+FLASH_SECT, (void *)&ff1, sizeof(ff1)) != SPI_FLASH_RESULT_OK)
    memset(&ff1, 0, sizeof(ff1)); // clear in case of error
  // figure out which one is good
  flash_pri = selectPrimary(&ff0, &ff1);
  // if neither is OK, we revert to defaults
  if (flash_pri < 0) {
    memcpy(&flashConfig, &flashDefault, sizeof(FlashConfig));
    flash_pri = 0;
    return false;
  }
  // copy good one into global var and return
  memcpy(&flashConfig, flash_pri == 0 ? &ff0.fc : &ff1.fc, sizeof(FlashConfig));
  return true;
}

static uint32_t ICACHE_FLASH_ATTR selectPrimary(FlashFull *ff0, FlashFull *ff1) {
  // check CRC of ff0
  uint16_t crc = ff0->fc.crc;
  ff0->fc.crc = 0;
  bool ff0_crc_ok = inet_chksum(ff0, sizeof(FlashFull)) == crc;

  // check CRC of ff1
  crc = ff1->fc.crc;
  ff1->fc.crc = 0;
  bool ff1_crc_ok = inet_chksum(ff1, sizeof(FlashFull)) == crc;

  // decided which we like better
  if (ff0_crc_ok)
    if (!ff1_crc_ok || ff0->fc.seq >= ff1->fc.seq)
      return 0; // use first sector as primary
    else
      return 1; // second sector is newer
  else
    return ff1_crc_ok ? 1 : -1;
}
