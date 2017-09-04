
#include <esp8266.h>
#include "cgiwifi.h"
#include "cgi.h"
#include "config.h"
#include "sntp.h"
#include "cgimqtt.h"
#ifdef SYSLOG
#include "syslog.h"
#endif
#include "time.h"
#include "cgiservices.h"

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

// Daylight Savings Time support
static ETSTimer dstTimer;
static bool old_dst;

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

static bool ICACHE_FLASH_ATTR isDSTEurope(struct tm *tp) {
  int mon = tp->tm_mon + 1;

  if (mon < 3 || tp->tm_mon > 11) return false;
  if (mon > 3 && tp->tm_mon < 11) return true;

  int previousSunday = tp->tm_mday - tp->tm_wday;

  if (mon == 10) {
    if (previousSunday < 25)
      return true;
    if (tp->tm_wday > 0)
      return false;
    if (tp->tm_hour < 2)
      return true;
    return false;
  }
  if (mon == 3) {
    if (previousSunday < 25)
      return false;
    if (tp->tm_wday > 0)
      return true;
    if (tp->tm_hour < 2)
      return false;
    return true;
  }

  return true;
}

static bool ICACHE_FLASH_ATTR isDSTUSA(struct tm *tp) {
  int month = tp->tm_mon + 1;

  // January, february, and december are out.
  if (month < 3 || month > 11)
    return false;

  // April to October are in
  if (month > 3 && month < 11)
    return true;

  int previousSunday = tp->tm_mday - tp->tm_wday;

  // In march, we are DST if our previous sunday was on or after the 8th.
  if (month == 3)
    return previousSunday >= 8;

  // In november we must be before the first sunday to be dst.
  // That means the previous sunday must be before the 1st.
  return previousSunday <= 0;
}

/*
 * When adding more regions, this must be extended.
 * If someone needs a half hour timezone, then it might be better to change
 * the return type to int instead of bool.
 * It could then mean the number of minutes to add.
 */
static bool ICACHE_FLASH_ATTR isDST(struct tm *tp) {
  switch (flashConfig.dst_mode) {
  case DST_EUROPE:
    return isDSTEurope(tp);
  case DST_USA:
    return isDSTUSA(tp);
  case DST_NONE:
  default:
    return false;
  }
}

static ICACHE_FLASH_ATTR char *dstMode2text(int dm) {
  switch(dm) {
  case DST_EUROPE:
    return "Europe";
  case DST_USA:
    return "USA";
  case DST_NONE:
  default:
    return "None";
  }
}


#if 1
/*
 * For debugging only
 * This function is called yet another timeout after changing the timezone offset.
 * This is the only way to report the time correctly if we're in DST.
 */
static void ICACHE_FLASH_ATTR dstDelayed2() {
  time_t ts = sntp_get_current_timestamp();
  if (ts < 10000)
    return;
  struct tm *tp = gmtime(&ts);

  os_printf("dstDelayed (dst) : date %04d-%02d-%02d time %02d:%02d:%02d\n",
    tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec);
}
#endif

/*
 * This gets called a while after initializing SNTP.
 * The assumption is we will now know the date/time so we can determine whether we're in
 * a Daylight Savings Time period.
 * And then set the time offset accordingly.
 */
static void ICACHE_FLASH_ATTR dstDelayed() {
  time_t ts = sntp_get_current_timestamp();
  if (ts < 10000)
    return;		// FIX ME failed to obtain time. Now what ?

  struct tm *tp = gmtime(&ts);

  bool dst = isDST(tp);

  os_printf("dstDelayed : date %04d-%02d-%02d time %02d:%02d:%02d, wday = %d, DST = %s (%s)\n",
    tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec,
    tp->tm_wday, dst ? "true" : "false", dstMode2text(flashConfig.dst_mode));

  old_dst = dst;

  // Only change the timezone offset if we're in DST
  if (dst) {
    // Changing timezone offset requires a SNTP stop/start.
    sntp_stop();
    if (! sntp_set_timezone(flashConfig.timezone_offset + 1))
      os_printf("sntp_set_timezone(%d) failed\n", flashConfig.timezone_offset + 1);
    sntp_init();

#if 1
    // FIX ME for debugging only
    os_timer_disarm(&dstTimer);
    os_timer_setfn(&dstTimer, dstDelayed2, NULL);
    os_timer_arm(&dstTimer, 5000, 0);		// wait 5 seconds
#endif
  }
}

void ICACHE_FLASH_ATTR cgiServicesSNTPInit() {
  if (flashConfig.sntp_server[0] != '\0') {
    sntp_stop();
    if (true == sntp_set_timezone(flashConfig.timezone_offset)) {
      sntp_setservername(0, flashConfig.sntp_server);
      sntp_init();
    }
    old_dst = false;

    // If we support daylight savings time, delay setting the timezone until we know the date/time
    if (flashConfig.dst_mode != 0) {
      os_timer_disarm(&dstTimer);
      os_timer_setfn(&dstTimer, dstDelayed, NULL);
      os_timer_arm(&dstTimer, 5000, 0);		// wait 5 seconds

      DBG("SNTP timesource set to %s with offset %d, awaiting DST info\n",
        flashConfig.sntp_server, flashConfig.timezone_offset);
    } else {
      DBG("SNTP timesource set to %s with offset %d\n",
        flashConfig.sntp_server, flashConfig.timezone_offset);
    }
  }
}

/*
 * Check whether to change DST, at every CMD_GET_TIME call.
 *
 * If we need to change, then the next call to sntp_get_current_timestamp() will return 0
 * so a retry will be required. The apps already had to cope with such situations, I believe.
 */
void ICACHE_FLASH_ATTR cgiServicesCheckDST() {
  if (flashConfig.dst_mode == 0)
    return;

  time_t ts = sntp_get_current_timestamp();
  if (ts < 10000)
    return;		// FIX ME failed to obtain time. Now what ?

  struct tm *tp = gmtime(&ts);
  bool dst = isDST(tp);

  if (dst != old_dst) {
    // Just calling this would be easy but too slow, we already know the new DST setting.
    // cgiServicesSNTPInit();

    old_dst = dst;

    int add = dst ? 1 : 0;
    sntp_stop();
    if (! sntp_set_timezone(flashConfig.timezone_offset + add))
      os_printf("sntp_set_timezone(%d) failed\n", flashConfig.timezone_offset + add);
    sntp_init();

#if 1
    // FIX ME for debugging only
    os_timer_disarm(&dstTimer);
    os_timer_setfn(&dstTimer, dstDelayed2, NULL);
    os_timer_arm(&dstTimer, 5000, 0);		// wait 5 seconds
#endif
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
      "\"mdns_servername\": \"%s\", "
      "\"dst_mode\": \"%d\" "
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
    flashConfig.mdns_servername,
#if 0
    flashConfig.dst_mode ?
      (flashConfig.dst_mode == DST_EUROPE ? "Europe" : "USA")
      : "None"
#endif
    flashConfig.dst_mode
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

  sntp |= getUInt8Arg(connData, "dst_mode", &flashConfig.dst_mode);
  os_printf("DSTMODE %d\n", flashConfig.dst_mode);
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
