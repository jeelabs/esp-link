// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt

#include <esp8266.h>
#include "uart.h"
#include "cgi.h"
#include "uart.h"
#include "serbridge.h"
#include "config.h"
#include "console.h"

// Microcontroller console capturing the last 1024 characters received on the uart so
// they can be shown on a web page

// Buffer to hold concole contents.
// Invariants:
// - console_rd==console_wr <=> buffer empty
// - *console_rd == next char to read
// - *console_wr == next char to write
// - 0 <= console_xx < BUF_MAX
// - (console_wr+1)%BUF_MAX) == console_rd <=> buffer full
#define BUF_MAX (1024)
static char console_buf[BUF_MAX];
static int console_wr, console_rd;
static int console_pos; // offset since reset of buffer

static void ICACHE_FLASH_ATTR
console_write(char c) {
  console_buf[console_wr] = c;
  console_wr = (console_wr+1) % BUF_MAX;
  if (console_wr == console_rd) {
    // full, we write anyway and loose the oldest char
    console_rd = (console_rd+1) % BUF_MAX; // full, eat first char
    console_pos++;
  }
}

#if 0
// return previous character in console, 0 if at start
static char ICACHE_FLASH_ATTR
console_prev(void) {
  if (console_wr == console_rd) return 0;
  return console_buf[(console_wr-1+BUF_MAX)%BUF_MAX];
}
#endif

void ICACHE_FLASH_ATTR
console_write_char(char c) {
<<<<<<< HEAD
	//if (c == '\n' && console_prev() != '\r') console_write('\r'); // does more harm than good
	console_write(c);
=======
  //if (c == '\n' && console_prev() != '\r') console_write('\r'); // does more harm than good
  console_write(c);
>>>>>>> master
}

int ICACHE_FLASH_ATTR
ajaxConsoleReset(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  jsonHeader(connData, 200);
  console_rd = console_wr = console_pos = 0;
  serbridgeReset();
  return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR
ajaxConsoleBaud(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  char buff[512];
  int len, status = 400;
  len = httpdFindArg(connData->getArgs, "rate", buff, sizeof(buff));
  if (len > 0) {
    int rate = atoi(buff);
    if (rate >= 9600 && rate <= 1000000) {
      uart0_baud(rate);
      flashConfig.baud_rate = rate;
      status = configSave() ? 200 : 400;
    }
  } else if (connData->requestType == HTTPD_METHOD_GET) {
    status = 200;
  }

  jsonHeader(connData, status);
  os_sprintf(buff, "{\"rate\": %ld}", flashConfig.baud_rate);
  httpdSend(connData, buff, -1);
  return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR
ajaxConsole(HttpdConnData *connData) {
<<<<<<< HEAD
	if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
	char buff[2048];
	int len; // length of text in buff
	int console_len = (console_wr+BUF_MAX-console_rd) % BUF_MAX; // num chars in console_buf
	int start = 0; // offset onto console_wr to start sending out chars

	jsonHeader(connData, 200);

	// figure out where to start in buffer based on URI param
	len = httpdFindArg(connData->getArgs, "start", buff, sizeof(buff));
	if (len > 0) {
		start = atoi(buff);
		if (start < console_pos) {
			start = 0;
		} else if (start >= console_pos+console_len) {
			start = console_len;
		} else {
			start = start - console_pos;
		}
	}

	// start outputting
	len = os_sprintf(buff, "{\"len\":%d, \"start\":%d, \"text\": \"",
			console_len-start, console_pos+start);

	int rd = (console_rd+start) % BUF_MAX;
	while (len < 2040 && rd != console_wr) {
		uint8_t c = console_buf[rd];
		if (c == '\\' || c == '"') {
			buff[len++] = '\\';
			buff[len++] = c;
		} else if (c == '\r') {
			// this is crummy, but browsers display a newline for \r\n sequences
		} else if (c < ' ') {
			len += os_sprintf(buff+len, "\\u%04x", c);
		} else {
			buff[len++] = c;
		}
		rd = (rd + 1) % BUF_MAX;
	}
	os_strcpy(buff+len, "\"}"); len+=2;
	httpdSend(connData, buff, len);
	return HTTPD_CGI_DONE;
=======
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  char buff[2048];
  int len; // length of text in buff
  int console_len = (console_wr+BUF_MAX-console_rd) % BUF_MAX; // num chars in console_buf
  int start = 0; // offset onto console_wr to start sending out chars

  jsonHeader(connData, 200);

  // figure out where to start in buffer based on URI param
  len = httpdFindArg(connData->getArgs, "start", buff, sizeof(buff));
  if (len > 0) {
    start = atoi(buff);
    if (start < console_pos) {
      start = 0;
    } else if (start >= console_pos+console_len) {
      start = console_len;
    } else {
      start = start - console_pos;
    }
  }

  // start outputting
  len = os_sprintf(buff, "{\"len\":%d, \"start\":%d, \"text\": \"",
      console_len-start, console_pos+start);

  int rd = (console_rd+start) % BUF_MAX;
  while (len < 2040 && rd != console_wr) {
    uint8_t c = console_buf[rd];
    if (c == '\\' || c == '"') {
      buff[len++] = '\\';
      buff[len++] = c;
    } else if (c == '\r') {
      // this is crummy, but browsers display a newline for \r\n sequences
    } else if (c < ' ') {
      len += os_sprintf(buff+len, "\\u%04x", c);
    } else {
      buff[len++] = c;
    }
    rd = (rd + 1) % BUF_MAX;
  }
  os_strcpy(buff+len, "\"}"); len+=2;
  httpdSend(connData, buff, len);
  return HTTPD_CGI_DONE;
>>>>>>> master
}

void ICACHE_FLASH_ATTR consoleInit() {
  console_wr = 0;
  console_rd = 0;
}


