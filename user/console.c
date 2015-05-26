#include <esp8266.h>
#include "uart.h"
#include "cgi.h"
#include "console.h"

// Web console for the esp8266 to replace outputting to uart1.
// The web console has a 1KB circular in-memory buffer which os_printf prints into and
// the HTTP handler simply displays the buffer content on a web page.

#define BUF_MAX (1024)
static char console_buf[BUF_MAX];
static int console_wr, console_rd;
static bool console_no_uart; // start out printing to uart
static bool console_newline; // at start of a new line

void ICACHE_FLASH_ATTR
console_uart(bool enable) {
	if (!enable && !console_no_uart) {
		os_printf("Turning OFF uart console\n");
		os_delay_us(4*1000L); // time for uart to flush
		console_no_uart = !enable;
	} else if (enable && console_no_uart) {
		console_no_uart = !enable;
		os_printf("Turning ON uart console\n");
	}
}

static void ICACHE_FLASH_ATTR
console_write(char c) {
	int wr = (console_wr+1)%BUF_MAX;
	if (wr == console_rd)
		console_rd = (console_rd+1) % BUF_MAX; // full, eat first char
	console_buf[console_wr] = c;
	console_wr = wr;
}

#if 0
static char ICACHE_FLASH_ATTR
console_read(void) {
	char c = 0;
	if (console_rd != console_wr) {
		c = console_buf[console_rd];
		console_rd = (console_rd+1) % BUF_MAX;
	}
	return c;
}
#endif

static void ICACHE_FLASH_ATTR
console_write_char(char c) {
	// Uart output unless disabled
	if (!console_no_uart) {
		if (console_newline) {
			uart0_write_char('>');
			uart0_write_char(' ');
			console_newline = false;
		}
		uart0_write_char(c);
		console_newline = c == '\n';
	}
	// Store in console buffer
	if (c == '\n') console_write('\r');
	console_write(c);
}

//===== Display a web page with the console
int ICACHE_FLASH_ATTR
tplConsole(HttpdConnData *connData, char *token, void **arg) {
	if (token==NULL) return HTTPD_CGI_DONE;
	char buff[256];

	if (os_strcmp(token, "console") == 0) {
		if (console_wr > console_rd) {
			httpdSend(connData, console_buf+console_rd, console_wr-console_rd);
		} else if (console_rd != console_wr) {
			httpdSend(connData, console_buf+console_rd, BUF_MAX-console_rd);
			httpdSend(connData, console_buf, console_wr);
		}
	} else if (os_strcmp(token, "topnav")==0) {
		printNav(buff);
		httpdSend(connData, buff, -1);
	} else {
		httpdSend(connData, "Unknown\n", -1);
	}
	return HTTPD_CGI_DONE;
}

void ICACHE_FLASH_ATTR consoleInit() {
	console_wr = 0;
	console_rd = 0;
  os_install_putc1((void *)console_write_char);
}


