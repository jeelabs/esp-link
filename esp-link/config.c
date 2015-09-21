// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt
/* Configuration stored in flash */

#include <esp8266.h>
#include <osapi.h>
#include "config.h"
#include "espfs.h"
#include "crc16.h"

FlashConfig flashConfig;
FlashConfig flashDefault = {
  33, 0, 0,
  MCU_RESET_PIN, MCU_ISP_PIN, LED_CONN_PIN, LED_SERIAL_PIN,
  115200,
  "esp-link\0",                        // hostname
  0, 0x00ffffff, 0,                    // static ip, netmask, gateway
  0,                                   // log mode
  0,                                   // swap uart (don't by default)
  1, 0,                                // tcp_enable, rssi_enable
  "\0",                                // api_key
  0, 0, 0,                             // slip_enable, mqtt_enable, mqtt_status_enable
  2, 1,                                // mqtt_timeout, mqtt_clean_session
  1883, 60,                            // mqtt port, mqtt_keepalive
  "\0", "\0", "\0", "\0", "\0",        // mqtt host, client_id, user, password, status-topic
};

typedef union {
  FlashConfig fc;
  uint8_t     block[1024];
} FlashFull;

// magic number to recognize thet these are our flash settings as opposed to some random stuff
#define FLASH_MAGIC  (0xaa55)

// size of the setting sector
#define FLASH_SECT   (4096)

// address where to flash the settings: there are 16KB of reserved space at the end of the first
// flash partition, we use the upper 8KB (2 sectors)
#define FLASH_ADDR   (FLASH_SECT + FIRMWARE_SIZE + 2*FLASH_SECT)

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
  os_memset(&ff, 0, sizeof(ff));
  os_memcpy(&ff, &flashConfig, sizeof(FlashConfig));
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
  ff.fc.crc = crc16_data((unsigned char*)&ff, sizeof(ff), 0);
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
#ifdef CONFIG_DBG
  os_printf("*** Failed to save config ***\n");
#endif
  return false;
}

void ICACHE_FLASH_ATTR configWipe(void) {
  spi_flash_erase_sector(FLASH_ADDR>>12);
  spi_flash_erase_sector((FLASH_ADDR+FLASH_SECT)>>12);
}

static int ICACHE_FLASH_ATTR selectPrimary(FlashFull *fc0, FlashFull *fc1);

bool ICACHE_FLASH_ATTR configRestore(void) {
  FlashFull ff0, ff1;
  // read both flash sectors
  if (spi_flash_read(FLASH_ADDR, (void *)&ff0, sizeof(ff0)) != SPI_FLASH_RESULT_OK)
    os_memset(&ff0, 0, sizeof(ff0)); // clear in case of error
  if (spi_flash_read(FLASH_ADDR+FLASH_SECT, (void *)&ff1, sizeof(ff1)) != SPI_FLASH_RESULT_OK)
    os_memset(&ff1, 0, sizeof(ff1)); // clear in case of error
  // figure out which one is good
  flash_pri = selectPrimary(&ff0, &ff1);
  // if neither is OK, we revert to defaults
  if (flash_pri < 0) {
    os_memcpy(&flashConfig, &flashDefault, sizeof(FlashConfig));
    char chipIdStr[6];
    os_sprintf(chipIdStr, "%06x", system_get_chip_id());
#ifdef CHIP_IN_HOSTNAME
    char hostname[16];
    os_strcpy(hostname, "esp-link-");
    os_strcat(hostname, chipIdStr);
    os_memcpy(&flashConfig.hostname, hostname, os_strlen(hostname));
#endif
    os_memcpy(&flashConfig.mqtt_clientid, &flashConfig.hostname, os_strlen(flashConfig.hostname));
    os_memcpy(&flashConfig.mqtt_status_topic, &flashConfig.hostname, os_strlen(flashConfig.hostname));
    flash_pri = 0;
    return false;
  }
  // copy good one into global var and return
  os_memcpy(&flashConfig, flash_pri == 0 ? &ff0.fc : &ff1.fc, sizeof(FlashConfig));
  return true;
}

static int ICACHE_FLASH_ATTR selectPrimary(FlashFull *ff0, FlashFull *ff1) {
  // check CRC of ff0
  uint16_t crc = ff0->fc.crc;
  ff0->fc.crc = 0;
  bool ff0_crc_ok = crc16_data((unsigned char*)ff0, sizeof(FlashFull), 0) == crc;
#ifdef CONFIG_DBG
  os_printf("FLASH chk=0x%04x crc=0x%04x full_sz=%d sz=%d chip_sz=%d\n",
      crc16_data((unsigned char*)ff0, sizeof(FlashFull), 0),
      crc,
      sizeof(FlashFull),
      sizeof(FlashConfig),
      getFlashSize());
#endif

  // check CRC of ff1
  crc = ff1->fc.crc;
  ff1->fc.crc = 0;
  bool ff1_crc_ok = crc16_data((unsigned char*)ff1, sizeof(FlashFull), 0) == crc;

  // decided which we like better
  if (ff0_crc_ok)
    if (!ff1_crc_ok || ff0->fc.seq >= ff1->fc.seq)
      return 0; // use first sector as primary
    else
      return 1; // second sector is newer
  else
    return ff1_crc_ok ? 1 : -1;
}

// returns the flash chip's size, in BYTES
const size_t ICACHE_FLASH_ATTR
getFlashSize() {
  uint32_t id = spi_flash_get_id();
  uint8_t mfgr_id = id & 0xff;
  //uint8_t type_id = (id >> 8) & 0xff; // not relevant for size calculation
  uint8_t size_id = (id >> 16) & 0xff; // lucky for us, WinBond ID's their chips as a form that lets us calculate the size
  if (mfgr_id != 0xEF) // 0xEF is WinBond; that's all we care about (for now)
    return 0;
  return 1 << size_id;
}
