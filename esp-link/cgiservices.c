
#include <esp8266.h>
#include "cgiwifi.h"
#include "cgi.h"
#include "config.h"
#include "sntp.h"
#include "cgimqtt.h"
#ifdef SYSLOG
#include "syslog.h"
#endif

#ifdef CGISERVICES_DBG
#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)
#else
#define DBG(format, ...) do { } while(0)
#endif

char* rst_codes[7] = {
  "normal", "wdt reset", "exception", "soft wdt", "restart", "deep sleep", "external",
};

char* flash_maps[7] = {
  "512KB:256/256", "256KB", "1MB:512/512", "2MB:512/512", "4MB:512/512",
  "2MB:1024/1024", "4MB:1024/1024"
};

static ETSTimer reassTimer;

// Cgi to update system info (name/description)
int ICACHE_FLASH_ATTR cgiSystemSet(HttpdConnData *connData) {
  if (connData->conn == NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

  int8_t n = getStringArg(connData, "name", flashConfig.hostname, sizeof(flashConfig.hostname));
  int8_t d = getStringArg(connData, "description", flashConfig.sys_descr, sizeof(flashConfig.sys_descr));

  if (n < 0 || d < 0) return HTTPD_CGI_DONE; // getStringArg has produced an error response

  if (n > 0) {
    // schedule hostname change-over
    os_timer_disarm(&reassTimer);
    os_timer_setfn(&reassTimer, configWifiIP, NULL);
    os_timer_arm(&reassTimer, 1000, 0); // 1 second for the response of this request to make it
  }

  if (configSave()) {
    httpdStartResponse(connData, 204);
    httpdEndHeaders(connData);
  }
  else {
    httpdStartResponse(connData, 500);
    httpdEndHeaders(connData);
    httpdSend(connData, "Failed to save config", -1);
  }
  return HTTPD_CGI_DONE;
}

// Cgi to return various System information
int ICACHE_FLASH_ATTR cgiSystemInfo(HttpdConnData *connData) {
  char buff[1024];

  if (connData->conn == NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

  uint8 part_id = system_upgrade_userbin_check();
  uint32_t fid = spi_flash_get_id();
  struct rst_info *rst_info = system_get_rst_info();

  os_sprintf(buff,
    "{ "
      "\"name\": \"%s\", "
      "\"reset cause\": \"%d=%s\", "
      "\"size\": \"%s\", "
      "\"upload-size\": \"%d\", "
      "\"id\": \"0x%02X 0x%04X\", "
      "\"partition\": \"%s\", "
      "\"slip\": \"%s\", "
      "\"mqtt\": \"%s/%s\", "
      "\"baud\": \"%d\", "
      "\"description\": \"%s\""
    " }",
    flashConfig.hostname,
    rst_info->reason,
    rst_codes[rst_info->reason],
    flash_maps[system_get_flash_size_map()],
    getUserPageSectionEnd()-getUserPageSectionStart(),
    fid & 0xff, (fid & 0xff00) | ((fid >> 16) & 0xff),
    part_id ? "user2.bin" : "user1.bin",
    flashConfig.slip_enable ? "enabled" : "disabled",
    flashConfig.mqtt_enable ? "enabled" : "disabled",
    mqttState(),
    flashConfig.baud_rate,
    flashConfig.sys_descr
    );

  jsonHeader(connData, 200);
  httpdSend(connData, buff, -1);
  return HTTPD_CGI_DONE;
}

void ICACHE_FLASH_ATTR cgiServicesSNTPInit() {
  if (flashConfig.sntp_server[0] != '\0') {
    sntp_stop();
    if (true == sntp_set_timezone(flashConfig.timezone_offset)) {
      sntp_setservername(0, flashConfig.sntp_server);
      sntp_init();
    }
    DBG("SNTP timesource set to %s with offset %d\n", flashConfig.sntp_server, flashConfig.timezone_offset);
  }
}

int ICACHE_FLASH_ATTR cgiServicesInfo(HttpdConnData *connData) {
  char buff[1024];

  if (connData->conn == NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

  os_sprintf(buff,
    "{ "
#ifdef SYSLOG
      "\"syslog_host\": \"%s\", "
      "\"syslog_minheap\": %d, "
      "\"syslog_filter\": %d, "
      "\"syslog_showtick\": \"%s\", "
      "\"syslog_showdate\": \"%s\", "
#endif
      "\"timezone_offset\": %d, "
      "\"sntp_server\": \"%s\", "
      "\"mdns_enable\": \"%s\", "
      "\"mdns_servername\": \"%s\""
    " }",
#ifdef SYSLOG
    flashConfig.syslog_host,
    flashConfig.syslog_minheap,
    flashConfig.syslog_filter,
    flashConfig.syslog_showtick ? "enabled" : "disabled",
    flashConfig.syslog_showdate ? "enabled" : "disabled",
#endif
    flashConfig.timezone_offset,
    flashConfig.sntp_server,
    flashConfig.mdns_enable ? "enabled" : "disabled",
    flashConfig.mdns_servername
    );

  jsonHeader(connData, 200);
  httpdSend(connData, buff, -1);
  return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiServicesSet(HttpdConnData *connData) {
  if (connData->conn == NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

  int8_t syslog = 0;

  syslog |= getStringArg(connData, "syslog_host", flashConfig.syslog_host, sizeof(flashConfig.syslog_host));
  if (syslog < 0) return HTTPD_CGI_DONE;
  syslog |= getUInt16Arg(connData, "syslog_minheap", &flashConfig.syslog_minheap);
  if (syslog < 0) return HTTPD_CGI_DONE;
  syslog |= getUInt8Arg(connData, "syslog_filter", &flashConfig.syslog_filter);
  if (syslog < 0) return HTTPD_CGI_DONE;
  syslog |= getBoolArg(connData, "syslog_showtick", &flashConfig.syslog_showtick);
  if (syslog < 0) return HTTPD_CGI_DONE;
  syslog |= getBoolArg(connData, "syslog_showdate", &flashConfig.syslog_showdate);
  if (syslog < 0) return HTTPD_CGI_DONE;

#ifdef SYSLOG
  if (syslog > 0) {
    syslog_init(flashConfig.syslog_host);
  }
#endif

  int8_t sntp = 0;
  sntp |= getInt8Arg(connData, "timezone_offset", &flashConfig.timezone_offset);
  if (sntp < 0) return HTTPD_CGI_DONE;
  sntp |= getStringArg(connData, "sntp_server", flashConfig.sntp_server, sizeof(flashConfig.sntp_server));
  if (sntp < 0) return HTTPD_CGI_DONE;

  if (sntp > 0) {
    cgiServicesSNTPInit();
  }

  int8_t mdns = 0;
  mdns |= getBoolArg(connData, "mdns_enable", &flashConfig.mdns_enable);
  if (mdns < 0) return HTTPD_CGI_DONE;

  if (mdns > 0) {
    if (flashConfig.mdns_enable){
      DBG("Services: MDNS Enabled\n");
      struct ip_info ipconfig;
      wifi_get_ip_info(STATION_IF, &ipconfig);

      if (wifiState == wifiGotIP && ipconfig.ip.addr != 0) {
        wifiStartMDNS(ipconfig.ip);
      }
    }
    else {
      DBG("Services: MDNS Disabled\n");
      espconn_mdns_server_unregister();
      espconn_mdns_close();
      mdns_started = true;
    }
  }
  else {
    mdns |= getStringArg(connData, "mdns_servername", flashConfig.mdns_servername, sizeof(flashConfig.mdns_servername));
    if (mdns < 0) return HTTPD_CGI_DONE;

    if (mdns > 0 && mdns_started) {
      DBG("Services: MDNS Servername Updated\n");
      espconn_mdns_server_unregister();
      espconn_mdns_close();
      struct ip_info ipconfig;
      wifi_get_ip_info(STATION_IF, &ipconfig);

      if (wifiState == wifiGotIP && ipconfig.ip.addr != 0) {
        wifiStartMDNS(ipconfig.ip);
      }
    }
  }

  if (configSave()) {
    httpdStartResponse(connData, 204);
    httpdEndHeaders(connData);
  }
  else {
    httpdStartResponse(connData, 500);
    httpdEndHeaders(connData);
    httpdSend(connData, "Failed to save config", -1);
  }
  return HTTPD_CGI_DONE;
}
