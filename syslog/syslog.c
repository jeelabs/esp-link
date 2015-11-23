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
#include <syslog.h>
#include <time.h>
#include "task.h"

#define SYSLOG_DBG
#ifdef SYSLOG_DBG
#define DBG_SYSLOG(format, ...) os_printf(format, ## __VA_ARGS__)
#else
#define DBG_SYSLOG(format, ...) do { } while(0)
#endif

LOCAL os_timer_t check_udp_timer;
LOCAL struct espconn syslog_espconn;
LOCAL int syslog_msgid = 1;
LOCAL uint8_t syslog_task = 0;

LOCAL syslog_host_t	syslogHost;
LOCAL syslog_entry_t *syslogQueue = NULL;

static enum syslog_state syslogState = SYSLOG_NONE;

LOCAL void ICACHE_FLASH_ATTR syslog_add_entry(char *msg);
LOCAL void ICACHE_FLASH_ATTR syslog_check_udp(void);
LOCAL void ICACHE_FLASH_ATTR syslog_udp_sent_cb(void *arg);
#ifdef SYSLOG_UDP_RECV
LOCAL void ICACHE_FLASH_ATTR syslog_udp_recv_cb(void *arg, char *pusrdata, unsigned short length);
#endif

#define syslog_send_udp() post_usr_task(syslog_task,0)

/******************************************************************************
 * FunctionName : syslog_check_udp
 * Description  : check whether get ip addr or not
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
syslog_check_udp(void)
{
    struct ip_info ipconfig;
    DBG_SYSLOG("syslog_check_udp: state: %d ", syslogState);

   //disarm timer first
    os_timer_disarm(&check_udp_timer);

   //get ip info of ESP8266 station
    wifi_get_ip_info(STATION_IF, &ipconfig);
    if (wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0)
    {
    if (syslogState == SYSLOG_WAIT) {			// waiting for initialization
    DBG_SYSLOG("connected, initializing UDP socket\n");
    syslog_init(flashConfig.syslog_host);
    }
    } else {
    DBG_SYSLOG("waiting 100ms\n");
    if ((wifi_station_get_connect_status() == STATION_WRONG_PASSWORD ||
    wifi_station_get_connect_status() == STATION_NO_AP_FOUND ||
    wifi_station_get_connect_status() == STATION_CONNECT_FAIL)) {
    os_printf("*** connect failure!!! \r\n");
    } else {
    //re-arm timer to check ip
    os_timer_setfn(&check_udp_timer, (os_timer_func_t *)syslog_check_udp, NULL);
    os_timer_arm(&check_udp_timer, 100, 0);
    }
    }
    }

LOCAL void ICACHE_FLASH_ATTR
syslog_udp_send_event(os_event_t *events) {
  if (syslogQueue == NULL)
    syslogState = SYSLOG_READY;
  else {
    int res, len = os_strlen(syslogQueue->msg);
    DBG_SYSLOG("syslog_udp_send: %s\n", syslogQueue->msg);
    syslog_espconn.proto.udp->remote_port = syslogHost.port;			// ESP8266 udp remote port
    os_memcpy(&syslog_espconn.proto.udp->remote_ip, &syslogHost.addr.addr, 4);	// ESP8266 udp remote IP
    res = espconn_send(&syslog_espconn, (uint8_t *)syslogQueue->msg, len);
    if (res != 0)
      os_printf("syslog_udp_send: error %d\n", res);
  }
}

LOCAL void ICACHE_FLASH_ATTR
syslog_add_entry(char *msg)
{
	syslog_entry_t *se = NULL,
			*q = syslogQueue;

	// ensure we have sufficient heap for the rest of the system
	if (system_get_free_heap_size() > syslogHost.min_heap_size) {
	  se = os_zalloc(sizeof (syslog_entry_t) + os_strlen(msg) + 1);
	  os_strcpy(se->msg, msg);
	  se->next = NULL;

	  if (q == NULL)
	    syslogQueue = se;
	  else {
	    while (q->next != NULL)
	      q = q->next;
	    q->next = se;	// append msg to syslog queue
	  }
	} else {
	  if (syslogState != SYSLOG_HALTED) {
	    syslogState = SYSLOG_HALTED;
	    os_printf("syslog_add_entry: Warning: queue filled up, halted\n");
	    // add obit syslog message
	  }
	}
}

/******************************************************************************
      * FunctionName : syslog_sent_cb
      * Description  : udp sent successfully
      * 	       fetch next syslog package, free old message
      * Parameters  :  arg -- Additional argument to pass to the callback function
      * Returns      : none
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
syslog_udp_sent_cb(void *arg)
{
  struct espconn *pespconn = arg;
  DBG_SYSLOG("syslog_udp_sent_cb: %p\n", pespconn);

  // datagram is delivered - free and advance queue
  syslog_entry_t *p = syslogQueue;
  syslogQueue = syslogQueue -> next;
  os_free(p);

  if (syslogQueue != NULL)
    syslog_send_udp();
  else
    syslogState = SYSLOG_READY;
}

 /******************************************************************************
  * FunctionName : syslog_recv_cb
  * Description  : Processing the received udp packet
  * Parameters   : arg -- Additional argument to pass to the callback function
  *                pusrdata -- The received data (or NULL when the connection has been closed!)
  *                length -- The length of received data
  * Returns      : none
 *******************************************************************************/
#ifdef SYSLOG_UDP_RECV
LOCAL void ICACHE_FLASH_ATTR
syslog_udp_recv_cb(void *arg, char *pusrdata, unsigned short length)
{
  DBG_SYSLOG("syslog_udp_recv_cb: %p, %p, %d\n", arg, pusrdata, length);
}
#endif

 /******************************************************************************
  *
 *******************************************************************************/

 /******************************************************************************
  *
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
syslog_gethostbyname_cb(const char *name, ip_addr_t *ipaddr, void *arg)
{
  struct espconn *pespconn = (struct espconn *)arg;
  (void) pespconn;
  if (ipaddr != NULL) {
    syslogHost.addr.addr = ipaddr->addr;
    syslogState = SYSLOG_SENDING;
    syslog_send_udp();
  } else {
    syslogState = SYSLOG_ERROR;
    DBG_SYSLOG("syslog_gethostbyname_cb: state=SYSLOG_ERROR\n");
  }
}

 /******************************************************************************
  * FunctionName : initSyslog
  * Description  : Initialize the syslog library
  * Parameters   : hostname -- the syslog server (host:port)
  * 			   host:  IP-Addr | hostname
  * Returns      : none
 *******************************************************************************/
void ICACHE_FLASH_ATTR
syslog_init(char *syslog_server)
{
	char host[32], *port = &host[0];

	syslog_task = register_usr_task(syslog_udp_send_event);

	syslogState = SYSLOG_WAIT;
	syslogHost.min_heap_size = MINIMUM_HEAP_SIZE;
	syslogHost.port = 514;

	os_strncpy(host, syslog_server, 32);
	while (*port && *port != ':')			// find port delimiter
		port++;
	if (*port) {
		*port++ = '\0';
		syslogHost.port = atoi(port);
	}

	wifi_set_broadcast_if(STATIONAP_MODE); // send UDP broadcast from both station and soft-AP interface
	syslog_espconn.type = ESPCONN_UDP;
	syslog_espconn.proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
	syslog_espconn.proto.udp->local_port = espconn_port();				// set a available  port
#ifdef SYSLOG_UDP_RECV
	espconn_regist_recvcb(&syslog_espconn, syslog_udp_recv_cb);			// register a udp packet receiving callback
#endif
	espconn_regist_sentcb(&syslog_espconn, syslog_udp_sent_cb);			// register a udp packet sent callback
	espconn_create(&syslog_espconn);   						// create udp

	if (UTILS_StrToIP((const char *)host, (void*)&syslogHost.addr)) {
		syslogState = SYSLOG_SENDING;
		syslog_send_udp();
	} else {
		static struct espconn espconn_ghbn;
		espconn_gethostbyname(&espconn_ghbn, host, &syslogHost.addr, syslog_gethostbyname_cb);
		// syslog_send_udp is called by syslog_gethostbyname_cb()
	}
#ifdef SYSLOG_UDP_RECV
	DBG_SYSLOG("syslog_init: host: %s, port: %d, lport: %d, recvcb: %p, sentcb: %p, state: %d\n",
			host, syslogHost.port, syslog_espconn.proto.udp->local_port,
			syslog_udp_recv_cb, syslog_udp_sent_cb, syslogState	);
#else
	DBG_SYSLOG("syslog_init: host: %s, port: %d, lport: %d, rsentcb: %p, state: %d\n",
			host, syslogHost.port, syslog_espconn.proto.udp->local_port,
			syslog_udp_sent_cb, syslogState	);
#endif
}

 /******************************************************************************
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
void ICACHE_FLASH_ATTR
syslog(uint8_t facility, uint8_t severity, const char *tag, const char *fmt, ...)
{
	char udp_payload[1024],
			*p = udp_payload;
	uint32_t tick = WDEV_NOW();							// 0 ... 4294.967295s

	DBG_SYSLOG("syslog: state=%d ", syslogState);
	if (syslogState == SYSLOG_ERROR ||
		syslogState == SYSLOG_HALTED)
		return;

	// The Priority value is calculated by first multiplying the Facility
	// number by 8 and then adding the numerical value of the Severity.
	int sl = os_sprintf(p, "<%d> ", facility * 8 + severity);
	p += sl;

	// strftime doesn't work as expected - or adds 8k overhead.
	// so let's do poor man conversion - format is fixed anyway
	if (flashConfig.syslog_showdate == 0)
		sl = os_sprintf(p, "- ");
	else {
		time_t now = NULL;
		struct tm *tp = NULL;

		// create timestamp: FULL-DATE "T" PARTIAL-TIME "Z": 'YYYY-mm-ddTHH:MM:SSZ '
		// as long as realtime_stamp is 0 we use tick div 10⁶ as date
		now = (realtime_stamp == 0) ? (tick / 1000000) : realtime_stamp;
		tp = gmtime(&now);

		sl = os_sprintf(p, "%4d-%02d-%02dT%02d:%02d:%02dZ ",
			tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday,
			tp->tm_hour, tp->tm_min, tp->tm_sec);
		}
	p += sl;

	// add HOSTNAME APP-NAME PROCID MSGID
	if (flashConfig.syslog_showtick)
		sl = os_sprintf(p, "%s %s %lu.%06lu %d ", flashConfig.hostname, tag, tick / 1000000, tick % 1000000, syslog_msgid++);
	else
		sl = os_sprintf(p, "%s %s - %d ", flashConfig.hostname, tag, syslog_msgid++);
	p += sl;

	// append syslog message
	va_list arglist;
	va_start(arglist, fmt);
	sl = ets_vsprintf(p, fmt, arglist );
	va_end(arglist);

	DBG_SYSLOG("msg: %s\n", udp_payload);
	syslog_add_entry(udp_payload);	// add to message queue

	if (syslogState == SYSLOG_READY) {
	  syslogState = SYSLOG_SENDING;
	  syslog_send_udp();
	}

	if (syslogState == SYSLOG_NONE) {
	  //set a timer to check whether we got ip from router succeed or not.
	  os_timer_disarm(&check_udp_timer);
	  os_timer_setfn(&check_udp_timer, (os_timer_func_t *)syslog_check_udp, NULL);
	  os_timer_arm(&check_udp_timer, 100, 0);
	  syslogState = SYSLOG_WAIT;
	}
}
