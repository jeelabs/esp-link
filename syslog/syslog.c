/*
 * syslog.c
 *
 *
 * Copyright 2015 Susi's Strolch
 *
 * For license information see projects "License.txt"
 *
 */

#include <esp8266.h>
#include "config.h"
#include "syslog.h"
#include "time.h"
#include "task.h"
#include "sntp.h"

extern void * mem_trim(void *m, size_t s);	// not well documented...

#ifdef SYSLOG_DBG
#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)
#else
#define DBG(format, ...) do { } while(0)
#endif

#define WIFI_CHK_INTERVAL 1000	// ms to check Wifi statis

static struct espconn *syslog_espconn = NULL;
static uint32_t syslog_msgid = 1;
static uint8_t syslog_task = 0;

static syslog_host_t	syslogHost;
static syslog_entry_t *syslogQueue = NULL;

static enum syslog_state syslogState = SYSLOG_NONE;

static bool syslog_timer_armed = false;

static void ICACHE_FLASH_ATTR syslog_add_entry(syslog_entry_t *entry);
static void ICACHE_FLASH_ATTR syslog_chk_status(void);
static void ICACHE_FLASH_ATTR syslog_udp_sent_cb(void *arg);
static syslog_entry_t ICACHE_FLASH_ATTR *syslog_compose(uint8_t facility, uint8_t severity, const char *tag, const char *fmt, ...);

#ifdef SYSLOG_UDP_RECV
static void ICACHE_FLASH_ATTR syslog_udp_recv_cb(void *arg, char *pusrdata, unsigned short length);
#endif

#define syslog_send_udp() post_usr_task(syslog_task,0)

static char ICACHE_FLASH_ATTR *syslog_get_status(void) {
      switch (syslogState)
      {
        case SYSLOG_NONE:
          return "SYSLOG_NONE";
        case SYSLOG_WAIT:
          return "SYSLOG_WAIT";
        case SYSLOG_INIT:
          return "SYSLOG_INIT";
        case SYSLOG_INITDONE:
          return "SYSLOG_INITDONE";
        case SYSLOG_DNSWAIT:
          return "SYSLOG_DNSWAIT";
        case SYSLOG_READY:
          return "SYSLOG_READY";
        case SYSLOG_SENDING:
          return "SYSLOG_SENDING";
        case SYSLOG_SEND:
          return "SYSLOG_SEND";
        case SYSLOG_SENT:
          return "SYSLOG_SENT";
        case SYSLOG_HALTED:
          return "SYSLOG_HALTED";
        case SYSLOG_ERROR:
          return "SYSLOG_ERROR";
        default:
          break;
      }
      return "UNKNOWN ";
}

static void ICACHE_FLASH_ATTR syslog_set_status(enum syslog_state state) {
  syslogState = state;
  DBG("[%dµs] %s: %s (%d)\n", WDEV_NOW(), __FUNCTION__, syslog_get_status(), state);
#ifndef SYSLOG_DBG
  os_printf("Syslog state: %s\n", syslog_get_status());
#endif
}

static void ICACHE_FLASH_ATTR syslog_timer_arm(int delay) {
    static os_timer_t wifi_chk_timer = {};
    syslog_timer_armed = true;
    os_timer_disarm(&wifi_chk_timer);
    os_timer_setfn(&wifi_chk_timer, (os_timer_func_t *)syslog_chk_status, NULL);
    os_timer_arm(&wifi_chk_timer, delay, 0);
}

/******************************************************************************
 * FunctionName : syslog_chk_status
 * Description  : check whether get ip addr or not
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
static void ICACHE_FLASH_ATTR syslog_chk_status(void)
{
  struct ip_info ipconfig;

  DBG("[%uµs] %s: id=%lu ", WDEV_NOW(), __FUNCTION__, syslogQueue ? syslogQueue->msgid : 0);

  //disarm timer first
  syslog_timer_armed = false;

  //try to get ip info of ESP8266 station
  wifi_get_ip_info(STATION_IF, &ipconfig);
  int wifi_status = wifi_station_get_connect_status();
  if (wifi_status == STATION_GOT_IP && ipconfig.ip.addr != 0)
  {
    // it seems we have to add an additional delay after the Wifi is up and running.
    // so we simply add an intermediate state with 25ms delay
    switch (syslogState)
      {
        case SYSLOG_WAIT:
          DBG("%s: Wifi connected\n", syslog_get_status());
          syslog_set_status(SYSLOG_INIT);
          syslog_timer_arm(100);
          break;

        case SYSLOG_INIT:
          DBG("%s: init syslog\n", syslog_get_status());
          syslog_set_status(SYSLOG_INITDONE);
          syslog_init(flashConfig.syslog_host);
          syslog_timer_arm(10);
          break;

        case SYSLOG_DNSWAIT:
          DBG("%s: wait for DNS resolver\n", syslog_get_status());
          syslog_timer_arm(10);
          break;

        case SYSLOG_READY:
          DBG("%s: enforce sending\n", syslog_get_status());
          syslog_send_udp();
         break;

        case SYSLOG_SENDING:
          DBG("%s: delay\n", syslog_get_status());
          syslog_set_status(SYSLOG_SEND);
          syslog_timer_arm(2);
          break;

         case SYSLOG_SEND:
          DBG("%s: start sending\n", syslog_get_status());
          syslog_send_udp();
          break;

        default:
           DBG("%s: %d\n", syslog_get_status(), syslogState);
         break;
      }
  } else {
    if ((wifi_status == STATION_WRONG_PASSWORD ||
         wifi_status == STATION_NO_AP_FOUND ||
         wifi_status == STATION_CONNECT_FAIL)) {
      syslog_set_status(SYSLOG_ERROR);
      os_printf("*** connect failure!!!\n");
    } else {
      DBG("re-arming timer...\n");
      syslog_timer_arm(WIFI_CHK_INTERVAL);
    }
  }
}

/******************************************************************************
 * FunctionName : syslog_sent_cb
 * Description  : udp sent successfully
 * 	       fetch next syslog package, free old message
 * Parameters   :  arg -- Additional argument to pass to the callback function
 * Returns      : none
 ******************************************************************************/
static void ICACHE_FLASH_ATTR syslog_udp_sent_cb(void *arg)
{
  struct espconn *pespconn = arg;
  (void) pespconn;

  DBG("[%uµs] %s: id=%lu\n", WDEV_NOW(), __FUNCTION__, syslogQueue ? syslogQueue->msgid : 0);

  // datagram is delivered - free and advance queue
  syslog_entry_t *pse = syslogQueue;
  syslogQueue = syslogQueue -> next;
  os_free(pse);

  if (syslogQueue == NULL)
    syslog_set_status(SYSLOG_READY);
  else {
    // UDP seems timecritical - we must ensure a minimum delay after each package...
    syslog_set_status(SYSLOG_SENDING);
    if (! syslog_timer_armed)
    syslog_chk_status();
  }
}

static void ICACHE_FLASH_ATTR
syslog_udp_send_event(os_event_t *events) {
//  os_printf("syslog_udp_send_event: %d %lu, %lu\n", syslogState, syslogQueue->msgid, syslogQueue->tick);
  DBG("[%uµs] %s: id=%lu\n", WDEV_NOW(), __FUNCTION__, syslogQueue ? syslogQueue->msgid : 0);

  if (syslogQueue == NULL)
    syslog_set_status(SYSLOG_READY);
  else {
    int res = 0;
    syslog_espconn->proto.udp->remote_port = syslogHost.port;			// ESP8266 udp remote port
    os_memcpy(syslog_espconn->proto.udp->remote_ip, &syslogHost.addr.addr, 4);	// ESP8266 udp remote IP
    res = espconn_send(syslog_espconn, (uint8_t *)syslogQueue->datagram, syslogQueue->datagram_len);
    if (res != 0) {
      os_printf("syslog_udp_send: error %d\n", res);
    }
  }
}

 /*****************************************************************************
  * FunctionName : syslog_recv_cb
  * Description  : Processing the received udp packet
  * Parameters   : arg -- Additional argument to pass to the callback function
  *                pusrdata -- The received data (or NULL when the connection has been closed!)
  *                length -- The length of received data
  * Returns      : none
 ******************************************************************************/
#ifdef SYSLOG_UDP_RECV
static void ICACHE_FLASH_ATTR syslog_udp_recv_cb(void *arg, char *pusrdata, unsigned short length)
{
  DBG("syslog_udp_recv_cb: %p, %p, %d\n", arg, pusrdata, length);
}
#endif

/******************************************************************************
 *
 ******************************************************************************/
static void ICACHE_FLASH_ATTR syslog_gethostbyname_cb(const char *name, ip_addr_t *ipaddr, void *arg)
{
  struct espconn *pespconn = (struct espconn *)arg;
  (void) pespconn;

  DBG("[%uµs] %s\n", WDEV_NOW(), __FUNCTION__);
  if (ipaddr != NULL) {
    syslog(SYSLOG_FAC_USER, SYSLOG_PRIO_NOTICE, "SYSLOG",
          "resolved hostname: %s: " IPSTR, name, IP2STR(ipaddr));
    syslogHost.addr.addr = ipaddr->addr;
    syslog_set_status(SYSLOG_READY);
  } else {
    syslog_set_status(SYSLOG_ERROR);
    DBG("syslog_gethostbyname_cb: status=%s\n", syslog_get_status());
  }
  DBG("[%uµs] ex syslog_gethostbyname_cb()\n", WDEV_NOW());
}

 /******************************************************************************
  * FunctionName : initSyslog
  * Description  : Initialize the syslog library
  * Parameters   : syslog_host -- the syslog host (host:port)
  * 			   host:  IP-Addr | hostname
  * Returns      : none
 *******************************************************************************/
void ICACHE_FLASH_ATTR syslog_init(char *syslog_host)
{

  DBG("[%uµs] %s\n", WDEV_NOW(), __FUNCTION__);
  os_printf("SYSLOG host=%s *host=0x%x\n", syslog_host, *syslog_host);
  if (!*syslog_host) {
    syslog_set_status(SYSLOG_HALTED);
    return;
 }

  if (syslog_host == NULL) {
    // disable and unregister syslog handler
    syslog_set_status(SYSLOG_HALTED);
    if (syslog_espconn != NULL) {
      if (syslog_espconn->proto.udp) {
        // there's no counterpart to espconn_create...
        os_free(syslog_espconn->proto.udp);
      }
      os_free(syslog_espconn);
    }
    syslog_espconn = NULL;

    // clean up syslog queue
    syslog_entry_t *pse = syslogQueue;
    while (pse != NULL) {
      syslog_entry_t *next = pse->next;
      os_free(pse);
      pse = next;
    }
    syslogQueue = NULL;
    return;
  }

  char host[32], *port = &host[0];
  os_strncpy(host, syslog_host, 32);
  while (*port && *port != ':')			// find port delimiter
    port++;

  if (*port) {
    *port++ = '\0';
    syslogHost.port = atoi(port);
  }

  if (syslogHost.port == 0)
    syslogHost.port = 514;

  // allocate structures, init syslog_handler
  if (syslog_espconn == NULL)
    syslog_espconn = (espconn *)os_zalloc(sizeof(espconn));

  if (syslog_espconn->proto.udp == NULL)
    syslog_espconn->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));

  syslog_espconn->type = ESPCONN_UDP;
  syslog_espconn->proto.udp->local_port = espconn_port();			// set a available  port
#ifdef SYSLOG_UDP_RECV
  espconn_regist_recvcb(syslog_espconn, syslog_udp_recv_cb);			// register a udp packet receiving callback
#endif
  espconn_regist_sentcb(syslog_espconn, syslog_udp_sent_cb);			// register a udp packet sent callback
  syslog_task = register_usr_task(syslog_udp_send_event);
  syslogHost.min_heap_size = flashConfig.syslog_minheap;

// the wifi_set_broadcast_if must be handled global in connection handler...
//  wifi_set_broadcast_if(STATIONAP_MODE); // send UDP broadcast from both station and soft-AP interface
  espconn_create(syslog_espconn);   						// create udp

  syslog(SYSLOG_FAC_USER, SYSLOG_PRIO_NOTICE, "SYSLOG",
              "syslogserver: %s:%d %d", host, syslogHost.port, syslog_espconn->proto.udp->local_port);

  if (UTILS_StrToIP((const char *)host, (void*)&syslogHost.addr)) {
    syslog_set_status(SYSLOG_READY);
  } else {
    syslog_set_status(SYSLOG_DNSWAIT);
    syslog(SYSLOG_FAC_USER, SYSLOG_PRIO_NOTICE, "SYSLOG",
          "must resolve hostname \"%s\"", host);
    espconn_gethostbyname(syslog_espconn, host, &syslogHost.addr, syslog_gethostbyname_cb);
  }
}

/******************************************************************************
 * FunctionName : syslog_add_entry
 * Description  : add a syslog_entry_t to the syslogQueue
 * Parameters   : entry: the syslog_entry_t
 * Returns      : none
 ******************************************************************************/
static void ICACHE_FLASH_ATTR
syslog_add_entry(syslog_entry_t *entry)
{
  syslog_entry_t *pse = syslogQueue;

  DBG("[%dµs] %s id=%lu\n", WDEV_NOW(), __FUNCTION__, entry->msgid);
  // append msg to syslog_queue
  if (pse == NULL)
    syslogQueue = entry;
  else {
    while (pse->next != NULL)
      pse = pse->next;
    pse->next = entry;	// append msg to syslog queue
  }
//   DBG("%p %lu %d\n", entry, entry->msgid, system_get_free_heap_size());

  // ensure we have sufficient heap for the rest of the system
  if (system_get_free_heap_size() < syslogHost.min_heap_size) {
    if (syslogState != SYSLOG_HALTED) {
      os_printf("syslog_add_entry: Warning: queue filled up, halted\n");
      entry->next = syslog_compose(SYSLOG_FAC_USER, SYSLOG_PRIO_CRIT, "SYSLOG", "queue filled up, halted");
      if (syslogState == SYSLOG_READY)
        syslog_send_udp();
      syslog_set_status(SYSLOG_HALTED);
    }
  }
}

/******************************************************************************
 * FunctionName : syslog_compose
 * Description  : compose a syslog_entry_t from va_args
 * Parameters   : va_args
 * Returns      : the malloced syslog_entry_t
 ******************************************************************************/
LOCAL syslog_entry_t ICACHE_FLASH_ATTR *
syslog_compose(uint8_t facility, uint8_t severity, const char *tag, const char *fmt, ...)
{
  DBG("[%dµs] %s id=%lu\n", WDEV_NOW(), __FUNCTION__, syslog_msgid);
  syslog_entry_t *se = os_zalloc(sizeof (syslog_entry_t) + 1024);	// allow up to 1k datagram
  if (se == NULL) return NULL;
  char *p = se->datagram;
  se->tick = WDEV_NOW();			// 0 ... 4294.967295s
  se->msgid = syslog_msgid;

  // The Priority value is calculated by first multiplying the Facility
  // number by 8 and then adding the numerical value of the Severity.
  p += os_sprintf(p, "<%d> ", facility * 8 + severity);

  // strftime doesn't work as expected - or adds 8k overhead.
  // so let's do poor man conversion - format is fixed anyway
  if (flashConfig.syslog_showdate == 0)
    p += os_sprintf(p, "- ");
  else {
    time_t now = NULL;
    struct tm *tp = NULL;

    // create timestamp: FULL-DATE "T" PARTIAL-TIME "Z": 'YYYY-mm-ddTHH:MM:SSZ '
    // as long as realtime_stamp is 0 we use tick div 10⁶ as date
    uint32_t realtime_stamp = sntp_get_current_timestamp();
    now = (realtime_stamp == 0) ? (se->tick / 1000000) : realtime_stamp;
    tp = gmtime(&now);

    p += os_sprintf(p, "%4d-%02d-%02dT%02d:%02d:%02d",
		    tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday,
        tp->tm_hour, tp->tm_min, tp->tm_sec);
    if (realtime_stamp == 0)
      p += os_sprintf(p, ".%06uZ ", se->tick % 1000000);
    else
      p += os_sprintf(p, "%+03d:00 ", flashConfig.timezone_offset);
  }

  // add HOSTNAME APP-NAME PROCID MSGID
  if (flashConfig.syslog_showtick)
    p += os_sprintf(p, "%s %s %u.%06u %u ", flashConfig.hostname, tag, se->tick / 1000000,
        se->tick % 1000000, syslog_msgid++);
  else
    p += os_sprintf(p, "%s %s - %u ", flashConfig.hostname, tag, syslog_msgid++);

  // append syslog message
  va_list arglist;
  va_start(arglist, fmt);
  p += ets_vsprintf(p, fmt, arglist );
  va_end(arglist);

  se->datagram_len = p - se->datagram;
  se = mem_trim(se, sizeof(syslog_entry_t) + se->datagram_len + 1);
  return se;
}

 /*****************************************************************************
  * FunctionName : syslog
  * Description  : compose and queue a new syslog message
  * Parameters   : facility
  * 				severity
  * 				tag
  * 				message
  * 				...
  *
  *	  SYSLOG-MSG      = HEADER SP STRUCTURED-DATA [SP MSG]

      HEADER          = PRI VERSION SP TIMESTAMP SP HOSTNAME
                        SP APP-NAME SP PROCID SP MSGID
      PRI             = "<" PRIVAL ">"
      PRIVAL          = 1*3DIGIT ; range 0 .. 191
      VERSION         = NONZERO-DIGIT 0*2DIGIT
      HOSTNAME        = NILVALUE / 1*255PRINTUSASCII

      APP-NAME        = NILVALUE / 1*48PRINTUSASCII
      PROCID          = NILVALUE / 1*128PRINTUSASCII
      MSGID           = NILVALUE / 1*32PRINTUSASCII

      TIMESTAMP       = NILVALUE / FULL-DATE "T" FULL-TIME
      FULL-DATE       = DATE-FULLYEAR "-" DATE-MONTH "-" DATE-MDAY
      DATE-FULLYEAR   = 4DIGIT
      DATE-MONTH      = 2DIGIT  ; 01-12
      DATE-MDAY       = 2DIGIT  ; 01-28, 01-29, 01-30, 01-31 based on
                                ; month/year
      FULL-TIME       = PARTIAL-TIME TIME-OFFSET
      PARTIAL-TIME    = TIME-HOUR ":" TIME-MINUTE ":" TIME-SECOND
                        [TIME-SECFRAC]
      TIME-HOUR       = 2DIGIT  ; 00-23
      TIME-MINUTE     = 2DIGIT  ; 00-59
      TIME-SECOND     = 2DIGIT  ; 00-59
      TIME-SECFRAC    = "." 1*6DIGIT
      TIME-OFFSET     = "Z" / TIME-NUMOFFSET
      TIME-NUMOFFSET  = ("+" / "-") TIME-HOUR ":" TIME-MINUTE


      STRUCTURED-DATA = NILVALUE / 1*SD-ELEMENT
      SD-ELEMENT      = "[" SD-ID *(SP SD-PARAM) "]"
      SD-PARAM        = PARAM-NAME "=" %d34 PARAM-VALUE %d34
      SD-ID           = SD-NAME
      PARAM-NAME      = SD-NAME
      PARAM-VALUE     = UTF-8-STRING ; characters '"', '\' and
                                     ; ']' MUST be escaped.
      SD-NAME         = 1*32PRINTUSASCII
                        ; except '=', SP, ']', %d34 (")

      MSG             = MSG-ANY / MSG-UTF8
      MSG-ANY         = *OCTET ; not starting with BOM
      MSG-UTF8        = BOM UTF-8-STRING
      BOM             = %xEF.BB.BF
      UTF-8-STRING    = *OCTET ; UTF-8 string as specified
                        ; in RFC 3629

      OCTET           = %d00-255
      SP              = %d32
      PRINTUSASCII    = %d33-126
      NONZERO-DIGIT   = %d49-57
      DIGIT           = %d48 / NONZERO-DIGIT
      NILVALUE        = "-"
      *
  * TIMESTAMP:	realtime_clock == 0 ? timertick / 10⁶ : realtime_clock
  * HOSTNAME	hostname
  * APPNAME:	ems-esp-link
  * PROCID:		timertick
  * MSGID:		NILVALUE
  *
  * Returns      : none
 *******************************************************************************/
void ICACHE_FLASH_ATTR syslog(uint8_t facility, uint8_t severity, const char *tag, const char *fmt, ...)
{
  DBG("[%dµs] %s status: %s\n", WDEV_NOW(), __FUNCTION__, syslog_get_status());

  if (flashConfig.syslog_host[0] == 0 || syslogState == SYSLOG_ERROR || syslogState == SYSLOG_HALTED)
    return;

  if (severity > flashConfig.syslog_filter)
    return;

  // compose the syslog message
  void *arg = __builtin_apply_args();
  void *res = __builtin_apply((void*)syslog_compose, arg, 128);
  if (res == NULL) return; // compose failed, probably due to malloc failure
  syslog_entry_t *se  = *(syslog_entry_t **)res;

  // and append it to the message queue
  syslog_add_entry(se);

  if (syslogState == SYSLOG_NONE)
    syslog_set_status(SYSLOG_WAIT);

  if (! syslog_timer_armed)
    syslog_chk_status();
}
