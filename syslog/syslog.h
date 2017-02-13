/*
 * syslog.h
 *
 *
 * Copyright 2015 Susi's Strolch
 *
 * For license information see projects "License.txt"
 *
 * part of syslog.c - client library
 *
 */


#ifndef _SYSLOG_H
#define _SYSLOG_H

#ifdef __cplusplus
extern "C" {
#endif

enum syslog_state {
    SYSLOG_NONE,        // not initialized
    SYSLOG_WAIT,        // waiting for Wifi
    SYSLOG_INIT,	// WIFI avail, must initialize
    SYSLOG_INITDONE,
    SYSLOG_DNSWAIT,	// WIFI avail, init done, waiting for DNS resolve
    SYSLOG_READY,       // Wifi established, ready to send
    SYSLOG_SENDING,     // UDP package on the air
    SYSLOG_SEND,
    SYSLOG_SENT,
    SYSLOG_HALTED,      // heap full, discard message
    SYSLOG_ERROR,
};

enum syslog_priority {
	SYSLOG_PRIO_EMERG,	/* system is unusable */
	SYSLOG_PRIO_ALERT,	/* action must be taken immediately */
	SYSLOG_PRIO_CRIT,	/* critical conditions */
	SYSLOG_PRIO_ERR,		/* error conditions */
	SYSLOG_PRIO_WARNING,	/* warning conditions */
	SYSLOG_PRIO_NOTICE,	/* normal but significant condition */
	SYSLOG_PRIO_INFO,	/* informational */
	SYSLOG_PRIO_DEBUG,	/* debug-level messages */
};

enum syslog_facility {
	SYSLOG_FAC_KERN,	/* kernel messages */
	SYSLOG_FAC_USER,	/* random user-level messages */
	SYSLOG_FAC_MAIL,	/* mail system */
	SYSLOG_FAC_DAEMON,	/* system daemons */
	SYSLOG_FAC_AUTH,	/* security/authorization messages */
	SYSLOG_FAC_SYSLOG,	/* messages generated internally by syslogd */
	SYSLOG_FAC_LPR,		/* line printer subsystem */
	SYSLOG_FAC_NEWS,	/* network news subsystem */
	SYSLOG_FAC_UUCP,	/* UUCP subsystem */
	SYSLOG_FAC_CRON,	/* clock daemon */
	SYSLOG_FAC_AUTHPRIV,/* security/authorization messages (private) */
	SYSLOG_FAC_FTP,		/* ftp daemon */
	SYSLOG_FAC_LOCAL0,	/* reserved for local use */
	SYSLOG_FAC_LOCAL1,	/* reserved for local use */
	SYSLOG_FAC_LOCAL2,	/* reserved for local use */
	SYSLOG_FAC_LOCAL3,	/* reserved for local use */
	SYSLOG_FAC_LOCAL4,	/* reserved for local use */
	SYSLOG_FAC_LOCAL5,	/* reserved for local use */
	SYSLOG_FAC_LOCAL6,	/* reserved for local use */
	SYSLOG_FAC_LOCAL7,	/* reserved for local use */
};

#define MINIMUM_HEAP_SIZE	8192
#define REG_READ(_r) (*(volatile uint32 *)(_r))
#define WDEV_NOW()   REG_READ(0x3ff20c00)

// This variable disappeared from lwip in SDK 2.0...
// extern uint32_t	realtime_stamp;     // 1sec NTP ticker

typedef struct syslog_host_t syslog_host_t;
struct syslog_host_t {
    uint32_t	min_heap_size;	// minimum allowed heap size when buffering
    ip_addr_t	addr;
    uint16_t	port;
};

// buffered syslog event - f.e. if network stack isn't up and running
typedef struct syslog_entry_t syslog_entry_t;
struct syslog_entry_t {
    syslog_entry_t *next;
    uint32_t	msgid;
    uint32_t	tick;
    uint16_t	datagram_len;
    char	datagram[];
};

syslog_host_t syslogserver;

void ICACHE_FLASH_ATTR syslog_init(char *syslog_host);
void ICACHE_FLASH_ATTR syslog(uint8_t facility, uint8_t severity, const char tag[], const char message[], ...);

// some convenience macros
#ifdef SYSLOG
// extern char *esp_link_version; // in user_main.c
#define LOG_DEBUG(format, ...) syslog(SYSLOG_FAC_USER, SYSLOG_PRIO_DEBUG, "esp_link", format, ## __VA_ARGS__ )
#define LOG_NOTICE(format, ...) syslog(SYSLOG_FAC_USER, SYSLOG_PRIO_NOTICE, "esp_link", format, ## __VA_ARGS__ )
#define LOG_INFO(format, ...) syslog(SYSLOG_FAC_USER, SYSLOG_PRIO_INFO, "esp_link", format, ## __VA_ARGS__ )
#define LOG_WARN(format, ...) syslog(SYSLOG_FAC_USER, SYSLOG_PRIO_WARNING, "esp_link", format, ## __VA_ARGS__ )
#define LOG_ERR(format, ...) syslog(SYSLOG_FAC_USER, SYSLOG_PRIO_ERR, "esp_link", format, ## __VA_ARGS__ )
#else
#define LOG_DEBUG(format, ...) do { } while(0)
#define LOG_NOTICE(format, ...) do { } while(0)
#define LOG_WARN(format, ...) do { } while(0)
#define LOG_INFO(format, ...) do { } while(0)
#define LOG_ERR(format, ...) do { } while(0)
#endif

#ifdef __cplusplus
}
#endif

#endif	/* _SYSLOG_H */
