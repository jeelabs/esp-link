#include <esp8266.h>
#include "console.h"

// Web console for the esp8266 to replace outputting to uart1.
// The web console has a 1KB circular in-memory buffer which os_printf prints into and
// the HTTP handler simply displays the buffer content on a web page.

#define BUF_MAX (1024)
static char console_buf[BUF_MAX];
static int console_wr, console_rd;

static void ICACHE_FLASH_ATTR
console_write(char c) {
	int wr = (console_wr+1)%BUF_MAX;
	if (wr != console_rd) {
		console_buf[console_wr] = c;
		console_wr = wr;
	}
}

static char ICACHE_FLASH_ATTR
console_read(void) {
	char c = 0;
	if (console_rd != console_wr) {
		c = console_buf[console_rd];
		console_rd = (console_rd+1) % BUF_MAX;
	}
	return c;
}

static void ICACHE_FLASH_ATTR
console_write_char(char c) {
	if (c == '\n') console_write('\r');
	console_write(c);
}

//===== Display a web page with the console
int ICACHE_FLASH_ATTR
tplConsole(HttpdConnData *connData, char *token, void **arg) {
	if (token==NULL) return HTTPD_CGI_DONE;

	if (os_strcmp(token, "console") == 0) {
		char buf[128];
		int n = 0;
		while (console_rd != console_wr) {
			buf[n++] = console_read();
			if (n == 128) {
				httpdSend(connData, buf, n);
				n = 0;
			}
		}
		if (n > 0) httpdSend(connData, buf, n);
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


