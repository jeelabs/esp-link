#include <esp8266.h>
#include "uart.h"
#include "cgi.h"
#include "log.h"

// Web log for the esp8266 to replace outputting to uart1.
// The web log has a 1KB circular in-memory buffer which os_printf prints into and
// the HTTP handler simply displays the buffer content on a web page.

// see consolse.c for invariants (same here)
#define BUF_MAX (1024)
static char log_buf[BUF_MAX];
static int log_wr, log_rd;
static bool log_no_uart; // start out printing to uart
static bool log_newline; // at start of a new line

void ICACHE_FLASH_ATTR
log_uart(bool enable) {
	if (!enable && !log_no_uart) {
		os_printf("Turning OFF uart log\n");
		os_delay_us(4*1000L); // time for uart to flush
		log_no_uart = !enable;
	} else if (enable && log_no_uart) {
		log_no_uart = !enable;
		os_printf("Turning ON uart log\n");
	}
}

static void ICACHE_FLASH_ATTR
log_write(char c) {
	log_buf[log_wr] = c;
	log_wr = (log_wr+1) % BUF_MAX;
	if (log_wr == log_rd)
		log_rd = (log_rd+1) % BUF_MAX; // full, eat first char
}

#if 0
static char ICACHE_FLASH_ATTR
log_read(void) {
	char c = 0;
	if (log_rd != log_wr) {
		c = log_buf[log_rd];
		log_rd = (log_rd+1) % BUF_MAX;
	}
	return c;
}
#endif

static void ICACHE_FLASH_ATTR
log_write_char(char c) {
	// Uart output unless disabled
	if (!log_no_uart) {
		if (log_newline) {
			uart0_write_char('>');
			uart0_write_char(' ');
			log_newline = false;
		}
		uart0_write_char(c);
		log_newline = c == '\n';
	}
	// Store in log buffer
	if (c == '\n') log_write('\r');
	log_write(c);
}

//===== Display a web page with the log
int ICACHE_FLASH_ATTR
tplLog(HttpdConnData *connData, char *token, void **arg) {
	if (token==NULL) return HTTPD_CGI_DONE;

	if (os_strcmp(token, "log") == 0) {
		if (log_wr > log_rd) {
			httpdSend(connData, log_buf+log_rd, log_wr-log_rd);
		} else if (log_rd != log_wr) {
			httpdSend(connData, log_buf+log_rd, BUF_MAX-log_rd);
			httpdSend(connData, log_buf, log_wr);
		} else {
			httpdSend(connData, "<buffer empty>", -1);
		}
	} else if (os_strcmp(token, "head")==0) {
		printHead(connData);
	} else {
		httpdSend(connData, "Unknown\n", -1);
	}
	return HTTPD_CGI_DONE;
}

void ICACHE_FLASH_ATTR logInit() {
	log_wr = 0;
	log_rd = 0;
  os_install_putc1((void *)log_write_char);
}


