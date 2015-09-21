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

void noCacheHeaders(HttpdConnData *connData, int code) {
  httpdStartResponse(connData, code);
  httpdHeader(connData, "Cache-Control", "no-cache, no-store, must-revalidate");
  httpdHeader(connData, "Pragma", "no-cache");
  httpdHeader(connData, "Expires", "0");
}

void ICACHE_FLASH_ATTR
jsonHeader(HttpdConnData *connData, int code) {
  noCacheHeaders(connData, code);
  httpdHeader(connData, "Content-Type", "application/json");
  httpdEndHeaders(connData);
}

void ICACHE_FLASH_ATTR
errorResponse(HttpdConnData *connData, int code, char *message) {
  noCacheHeaders(connData, code);
  httpdEndHeaders(connData);
  httpdSend(connData, message, -1);
#ifdef CGI_DBG
  os_printf("HTTP %d error response: \"%s\"\n", code, message);
#endif
}

// look for the HTTP arg 'name' and store it at 'config' with max length 'max_len' (incl
// terminating zero), returns -1 on error, 0 if not found, 1 if found and OK
int8_t ICACHE_FLASH_ATTR
getStringArg(HttpdConnData *connData, char *name, char *config, int max_len) {
  char buff[128];
  int len = httpdFindArg(connData->getArgs, name, buff, sizeof(buff));
  if (len < 0) return 0; // not found, skip
  if (len >= max_len) {
    os_sprintf(buff, "Value for %s too long (%d > %d allowed)", name, len, max_len-1);
    errorResponse(connData, 400, buff);
    return -1;
  }
  strcpy(config, buff);
  return 1;
}

int8_t ICACHE_FLASH_ATTR
getBoolArg(HttpdConnData *connData, char *name, bool*config) {
  char buff[64];
  int len = httpdFindArg(connData->getArgs, name, buff, sizeof(buff));
  if (len < 0) return 0; // not found, skip

  if (strcmp(buff, "1") == 0 || strcmp(buff, "true") == 0) {
    *config = true;
    return 1;
      }
  if (strcmp(buff, "0") == 0 || strcmp(buff, "false") == 0) {
    *config = false;
    return 1;
      }
  os_sprintf(buff, "Invalid value for %s", name);
  errorResponse(connData, 400, buff);
  return -1;
}

uint8_t ICACHE_FLASH_ATTR
UTILS_StrToIP(const char* str, void *ip){
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

#define TOKEN(x) (os_strcmp(token, x) == 0)
#if 0
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
  httpdStartResponse(connData, 200);
  httpdHeader(connData, "Cache-Control", "max-age=3600, must-revalidate");
  httpdHeader(connData, "Content-Type", "application/json");
  httpdEndHeaders(connData);
  // construct json response
  os_sprintf(buff,
      "{\"menu\": [\"Home\", \"/home.html\", "
      "\"Wifi\", \"/wifi/wifi.html\","
      "\"\xC2\xB5" "C Console\", \"/console.html\", "
#ifdef MQTT
      "\"REST/MQTT\", \"/mqtt.html\","
#endif
      "\"Debug log\", \"/log.html\" ],\n"
      " \"version\": \"%s\" }", esp_link_version);
  httpdSend(connData, buff, -1);
  return HTTPD_CGI_DONE;
}
