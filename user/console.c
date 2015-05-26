#include <esp8266.h>
#include "uart.h"
#include "cgi.h"
#include "console.h"

// Microcontroller console capturing the last 1024 characters received on the uart so
// they can be shown on a web page

#define BUF_MAX (1024)
static char console_buf[BUF_MAX];
static int console_wr, console_rd;
static int console_pos; // offset since reset of console_rd position

static void ICACHE_FLASH_ATTR
console_write(char c) {
	int wr = (console_wr+1)%BUF_MAX;
	if (wr == console_rd) {
		console_rd = (console_rd+1) % BUF_MAX; // full, eat first char
		console_pos++;
	}
	console_buf[console_wr] = c;
	console_wr = wr;
}

// return previous character in console, 0 if at start
static char ICACHE_FLASH_ATTR
console_prev(void) {
	if (console_wr == console_rd) return 0;
	return console_buf[(console_wr-1+BUF_MAX)%BUF_MAX];
}

void ICACHE_FLASH_ATTR
console_write_char(char c) {
	if (c == '\n' && console_prev() != '\r') console_write('\r');
	console_write(c);
}

//===== Display a web page with the console
int ICACHE_FLASH_ATTR
tplConsole(HttpdConnData *connData, char *token, void **arg) {
	if (token==NULL) return HTTPD_CGI_DONE;

	if (os_strcmp(token, "console") == 0) {
		if (console_wr > console_rd) {
			httpdSend(connData, console_buf+console_rd, console_wr-console_rd);
		} else if (console_rd != console_wr) {
			httpdSend(connData, console_buf+console_rd, BUF_MAX-console_rd);
			httpdSend(connData, console_buf, console_wr);
		}
	} else if (os_strcmp(token, "head")==0) {
		printHead(connData);
	} else {
		httpdSend(connData, "Unknown\n", -1);
	}
	return HTTPD_CGI_DONE;
}

void ICACHE_FLASH_ATTR consoleInit() {
	console_wr = 0;
	console_rd = 0;
}


