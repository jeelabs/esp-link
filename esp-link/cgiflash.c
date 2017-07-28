/*
Some flash handling cgi routines. Used for reading the existing flash and updating the ESPFS image.
*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain
 * this notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 * Heavily modified and enhanced by Thorsten von Eicken in 2015
 * ----------------------------------------------------------------------------
 */

#include <esp8266.h>
#include "cgi.h"
#include "cgiflash.h"

#ifdef CGIFLASH_DBG
#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)
#else
#define DBG(format, ...) do { } while(0)
#endif

// Check that the header of the firmware blob looks like actual firmware...
static char* ICACHE_FLASH_ATTR check_header(void *buf) {
  uint8_t *cd = (uint8_t *)buf;
#ifdef CGIFLASH_DBG
  uint32_t *buf32 = buf;
  os_printf("%p: %08X %08X %08X %08X\n", buf, buf32[0], buf32[1], buf32[2], buf32[3]);
#endif
  if (cd[0] != 0xEA) return "IROM magic missing";
  if (cd[1] != 4 || cd[2] > 3 || (cd[3]>>4) > 6) return "bad flash header";
  if (((uint16_t *)buf)[3] != 0x4010) return "Invalid entry addr";
  if (((uint32_t *)buf)[2] != 0) return "Invalid start offset";
  return NULL;
}

// check whether the flash map/size we have allows for OTA upgrade
static bool canOTA(void) {
        enum flash_size_map map = system_get_flash_size_map();
        return map >= FLASH_SIZE_8M_MAP_512_512;
}

static char *flash_too_small = "Flash too small for OTA update";

//===== Cgi to query which firmware needs to be uploaded next
int ICACHE_FLASH_ATTR cgiGetFirmwareNext(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

        if (!canOTA()) {
          errorResponse(connData, 400, flash_too_small);
          return HTTPD_CGI_DONE;
        }

  uint8 id = system_upgrade_userbin_check();
  httpdStartResponse(connData, 200);
  httpdHeader(connData, "Content-Type", "text/plain");
  httpdHeader(connData, "Content-Length", "9");
  httpdEndHeaders(connData);
  char *next = id == 1 ? "user1.bin" : "user2.bin";
  httpdSend(connData, next, -1);
  DBG("Next firmware: %s (got %d)\n", next, id);
  return HTTPD_CGI_DONE;
}

//===== Cgi that allows the firmware to be replaced via http POST
int ICACHE_FLASH_ATTR cgiUploadFirmware(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

  if (!canOTA()) {
    errorResponse(connData, 400, flash_too_small);
    return HTTPD_CGI_DONE;
  }

  int offset = connData->post->received - connData->post->buffLen;
  if (offset == 0) {
    connData->cgiPrivData = NULL;
  } else if (connData->cgiPrivData != NULL) {
    // we have an error condition, do nothing
    return HTTPD_CGI_DONE;
  }

  // assume no error yet...
  char *err = NULL;
  int code = 400;

  // check overall size
  //os_printf("FW: %d (max %d)\n", connData->post->len, FIRMWARE_SIZE);
  if (connData->post->len > FIRMWARE_SIZE) err = "Firmware image too large";
  if (connData->post->buff == NULL || connData->requestType != HTTPD_METHOD_POST ||
      connData->post->len < 1024) err = "Invalid request";

  // check that data starts with an appropriate header
  if (err == NULL && offset == 0) err = check_header(connData->post->buff);

  // make sure we're buffering in 1024 byte chunks
  if (err == NULL && offset % 1024 != 0) {
    err = "Buffering problem";
    code = 500;
  }

  // return an error if there is one
  if (err != NULL) {
    DBG("Error %d: %s\n", code, err);
    httpdStartResponse(connData, code);
    httpdHeader(connData, "Content-Type", "text/plain");
    //httpdHeader(connData, "Content-Length", strlen(err)+2);
    httpdEndHeaders(connData);
    httpdSend(connData, err, -1);
    httpdSend(connData, "\r\n", -1);
    connData->cgiPrivData = (void *)1;
    return HTTPD_CGI_DONE;
  }

  // let's see which partition we need to flash and what flash address that puts us at
  uint8 id = system_upgrade_userbin_check();
  int address = id == 1 ? 4*1024                   // either start after 4KB boot partition
      : 4*1024 + FIRMWARE_SIZE + 16*1024 + 4*1024; // 4KB boot, fw1, 16KB user param, 4KB reserved
  address += offset;

  // erase next flash block if necessary
  if (address % SPI_FLASH_SEC_SIZE == 0){
    DBG("Flashing 0x%05x (id=%d)\n", address, 2 - id);
    spi_flash_erase_sector(address/SPI_FLASH_SEC_SIZE);
  }

  // Write the data
  //DBG("Writing %d bytes at 0x%05x (%d of %d)\n", connData->post->buffSize, address,
  //		connData->post->received, connData->post->len);
  spi_flash_write(address, (uint32 *)connData->post->buff, connData->post->buffLen);

  if (connData->post->received == connData->post->len){
    httpdStartResponse(connData, 200);
    httpdEndHeaders(connData);
    return HTTPD_CGI_DONE;
  } else {
    return HTTPD_CGI_MORE;
  }
}

static ETSTimer flash_reboot_timer;

// Handle request to reboot into the new firmware
int ICACHE_FLASH_ATTR cgiRebootFirmware(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

  if (!canOTA()) {
    errorResponse(connData, 400, flash_too_small);
    return HTTPD_CGI_DONE;
  }

  // sanity-check that the 'next' partition actually contains something that looks like
  // valid firmware
  uint8 id = system_upgrade_userbin_check();
  int address = id == 1 ? 4*1024                   // either start after 4KB boot partition
      : 4*1024 + FIRMWARE_SIZE + 16*1024 + 4*1024; // 4KB boot, fw1, 16KB user param, 4KB reserved
  uint32 buf[8];
  DBG("Checking %p\n", (void *)address);
  spi_flash_read(address, buf, sizeof(buf));
  char *err = check_header(buf);
  if (err != NULL) {
    DBG("Error %d: %s\n", 400, err);
    httpdStartResponse(connData, 400);
    httpdHeader(connData, "Content-Type", "text/plain");
    //httpdHeader(connData, "Content-Length", strlen(err)+2);
    httpdEndHeaders(connData);
    httpdSend(connData, err, -1);
    httpdSend(connData, "\r\n", -1);
    return HTTPD_CGI_DONE;
  }

  httpdStartResponse(connData, 200);
  httpdHeader(connData, "Content-Length", "0");
  httpdEndHeaders(connData);

  // Schedule a reboot
  system_upgrade_flag_set(UPGRADE_FLAG_FINISH);
  os_timer_disarm(&flash_reboot_timer);
  os_timer_setfn(&flash_reboot_timer, (os_timer_func_t *)system_upgrade_reboot, NULL);
  os_timer_arm(&flash_reboot_timer, 2000, 1);
  return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiReset(HttpdConnData *connData) {
  if (connData->conn == NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

  httpdStartResponse(connData, 200);
  httpdHeader(connData, "Content-Length", "0");
  httpdEndHeaders(connData);

  // Schedule a reboot
  os_timer_disarm(&flash_reboot_timer);
  os_timer_setfn(&flash_reboot_timer, (os_timer_func_t *)system_restart, NULL);
  os_timer_arm(&flash_reboot_timer, 2000, 1);
  return HTTPD_CGI_DONE;
}
