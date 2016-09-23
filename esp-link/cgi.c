/*
Some random cgi routines.
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
#include "config.h"
#include "web-server.h"

#ifdef CGI_DBG
#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)
#else
#define DBG(format, ...) do { } while(0)
#endif

void ICACHE_FLASH_ATTR noCacheHeaders(HttpdConnData *connData, int code) {
  httpdStartResponse(connData, code);
  httpdHeader(connData, "Cache-Control", "no-cache, no-store, must-revalidate");
  httpdHeader(connData, "Pragma", "no-cache");
  httpdHeader(connData, "Expires", "0");
}

void ICACHE_FLASH_ATTR jsonHeader(HttpdConnData *connData, int code) {
  noCacheHeaders(connData, code);
  httpdHeader(connData, "Content-Type", "application/json");
  httpdEndHeaders(connData);
}

void ICACHE_FLASH_ATTR errorResponse(HttpdConnData *connData, int code, char *message) {
  noCacheHeaders(connData, code);
  httpdEndHeaders(connData);
  httpdSend(connData, message, -1);
  DBG("HTTP %d error response: \"%s\"\n", code, message);
}

// look for the HTTP arg 'name' and store it at 'config' with max length 'max_len' (incl
// terminating zero), returns -1 on error, 0 if not found, 1 if found and OK
int8_t ICACHE_FLASH_ATTR getStringArg(HttpdConnData *connData, char *name, char *config, int max_len) {
  char buff[128];
  int len = httpdFindArg(connData->getArgs, name, buff, sizeof(buff));
  if (len < 0) return 0; // not found, skip
  if (len >= max_len) {
    os_sprintf(buff, "Value for %s too long (%d > %d allowed)", name, len, max_len-1);
    errorResponse(connData, 400, buff);
    return -1;
  }
  os_strcpy(config, buff);
  return 1;
}

// look for the HTTP arg 'name' and store it at 'config' as an 8-bit integer
// returns -1 on error, 0 if not found, 1 if found and OK
int8_t ICACHE_FLASH_ATTR getInt8Arg(HttpdConnData *connData, char *name, int8_t *config) {
  char buff[16];
  int len = httpdFindArg(connData->getArgs, name, buff, sizeof(buff));
  if (len < 0) return 0; // not found, skip
  int m = atoi(buff);
  if (len > 5 || m < -127 || m > 127) {
    os_sprintf(buff, "Value for %s out of range", name);
    errorResponse(connData, 400, buff);
    return -1;
  }
  *config = m;
  return 1;
}

// look for the HTTP arg 'name' and store it at 'config' as an unsigned 8-bit integer
// returns -1 on error, 0 if not found, 1 if found and OK
int8_t ICACHE_FLASH_ATTR getUInt8Arg(HttpdConnData *connData, char *name, uint8_t *config) {
  char buff[16];
  int len = httpdFindArg(connData->getArgs, name, buff, sizeof(buff));
  if (len < 0) return 0; // not found, skip
  int m = atoi(buff);
  if (len > 4 || m < 0 || m > 255) {
    os_sprintf(buff, "Value for %s out of range", name);
    errorResponse(connData, 400, buff);
    return -1;
  }
  *config = m;
  return 1;
}

// look for the HTTP arg 'name' and store it at 'config' as an unsigned 16-bit integer
// returns -1 on error, 0 if not found, 1 if found and OK
int8_t ICACHE_FLASH_ATTR getUInt16Arg(HttpdConnData *connData, char *name, uint16_t *config) {
  char buff[16];
  int len = httpdFindArg(connData->getArgs, name, buff, sizeof(buff));
  if (len < 0) return 0; // not found, skip
  int m = atoi(buff);
  if (len > 6 || m < 0 || m > 65535) {
    os_sprintf(buff, "Value for %s out of range", name);
    errorResponse(connData, 400, buff);
    return -1;
  }
  *config = m;
  return 1;
}

int8_t ICACHE_FLASH_ATTR getBoolArg(HttpdConnData *connData, char *name, uint8_t *config) {
  char buff[16];
  int len = httpdFindArg(connData->getArgs, name, buff, sizeof(buff));
  if (len < 0) return 0; // not found, skip

  if (os_strcmp(buff, "1") == 0 || os_strcmp(buff, "true") == 0) {
    *config = 1;
    return 1;
  }

  if (os_strcmp(buff, "0") == 0 || os_strcmp(buff, "false") == 0) {
    *config = 0;
    return 1;
  }

  os_sprintf(buff, "Invalid value for %s", name);
  errorResponse(connData, 400, buff);
  return -1;
}

uint8_t ICACHE_FLASH_ATTR UTILS_StrToIP(const char* str, void *ip){
  /* The count of the number of bytes processed. */
  int i;
  /* A pointer to the next digit to process. */
  const char * start;

  start = str;
  for (i = 0; i < 4; i++) {
    /* The digit being processed. */
    char c;
    /* The value of this byte. */
    int n = 0;
    while (1) {
      c = *start;
      start++;
      if (c >= '0' && c <= '9') {
        n *= 10;
        n += c - '0';
      }
      /* We insist on stopping at "." if we are still parsing
      the first, second, or third numbers. If we have reached
      the end of the numbers, we will allow any character. */
      else if ((i < 3 && c == '.') || i == 3) {
        break;
      }
      else {
        return 0;
      }
    }
    if (n >= 256) {
      return 0;
    }
    ((uint8_t*)ip)[i] = n;
  }
  return 1;
}

#if 0
#define TOKEN(x) (os_strcmp(token, x) == 0)
// Handle system information variables and print their value, returns the number of
// characters appended to buff
int ICACHE_FLASH_ATTR printGlobalInfo(char *buff, int buflen, char *token) {
  if (TOKEN("si_chip_id")) {
    return os_sprintf(buff, "0x%x", system_get_chip_id());
  } else if (TOKEN("si_freeheap")) {
    return os_sprintf(buff, "%dKB", system_get_free_heap_size()/1024);
  } else if (TOKEN("si_uptime")) {
    uint32 t = system_get_time() / 1000000; // in seconds
    return os_sprintf(buff, "%dd%dh%dm%ds", t/(24*3600), (t/(3600))%24, (t/60)%60, t%60);
  } else if (TOKEN("si_boot_version")) {
    return os_sprintf(buff, "%d", system_get_boot_version());
  } else if (TOKEN("si_boot_address")) {
    return os_sprintf(buff, "0x%x", system_get_userbin_addr());
  } else if (TOKEN("si_cpu_freq")) {
    return os_sprintf(buff, "%dMhz", system_get_cpu_freq());
  } else {
    return 0;
  }
}
#endif

extern char *esp_link_version; // in user_main.c

int ICACHE_FLASH_ATTR cgiMenu(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  char buff[1024];
  // don't use jsonHeader so the response does get cached
  noCacheHeaders(connData, 200);
  httpdHeader(connData, "Content-Type", "application/json");
  httpdEndHeaders(connData);
  // limit hostname to 12 chars
  char name[13];
  os_strncpy(name, flashConfig.hostname, 12);
  name[12] = 0;
  // construct json response
  os_sprintf(buff,
    "{ "
      "\"menu\": [ "
        "\"Home\", \"/home.html\", "
        "\"WiFi Station\", \"/wifi/wifiSta.html\", "
        "\"WiFi Soft-AP\", \"/wifi/wifiAp.html\", "
        "\"&#xb5;C Console\", \"/console.html\", "
        "\"Services\", \"/services.html\", "
#ifdef MQTT
        "\"REST/MQTT\", \"/mqtt.html\", "
#endif
        "\"Debug log\", \"/log.html\","
        "\"Upgrade Firmware\", \"/flash.html\","
        "\"Web Server\", \"/web-server.html\""
	"%s"
      " ], "
      "\"version\": \"%s\", "
      "\"name\": \"%s\""
    " }",
  WEB_UserPages(), esp_link_version, name);

  httpdSend(connData, buff, -1);
  return HTTPD_CGI_DONE;
}
