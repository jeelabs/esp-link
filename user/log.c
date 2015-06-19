#include <esp8266.h>
#include "uart.h"
#include "cgi.h"
#include "log.h"

// Web log for the esp8266 to replace outputting to uart1.
// The web log has a 1KB circular in-memory buffer which os_printf prints into and
// the HTTP handler simply displays the buffer content on a web page.

// see console.c for invariants (same here)
#define BUF_MAX (1400)
static char log_buf[BUF_MAX];
static int log_wr, log_rd;
static int log_pos;
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
	if (log_wr == log_rd) {
		log_rd = (log_rd+1) % BUF_MAX; // full, eat first char
		log_pos++;
	}
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
			char buff[16];
			int l = os_sprintf(buff, "%6d> ", (system_get_time()/1000)%1000000);
			for (int i=0; i<l; i++)
				uart0_write_char(buff[i]);
			log_newline = false;
		}
		uart0_write_char(c);
		if (c == '\n') {
			log_newline = true;
			uart0_write_char('\r');
		}
	}
	// Store in log buffer
	if (c == '\n') log_write('\r');
	log_write(c);
}

int ICACHE_FLASH_ATTR
ajaxLog(HttpdConnData *connData) {
	char buff[2048];
	int len; // length of text in buff
	int log_len = (log_wr+BUF_MAX-log_rd) % BUF_MAX; // num chars in log_buf
	int start = 0; // offset onto log_wr to start sending out chars

	jsonHeader(connData, 200);

	// figure out where to start in buffer based on URI param
	len = httpdFindArg(connData->getArgs, "start", buff, sizeof(buff));
	if (len > 0) {
		start = atoi(buff);
		if (start < log_pos) {
			start = 0;
		} else if (start >= log_pos+log_len) {
			start = log_len;
		} else {
			start = start - log_pos;
		}
	}

	// start outputting
	len = os_sprintf(buff, "{\"len\":%d, \"start\":%d, \"text\": \"",
			log_len-start, log_pos+start);

	int rd = (log_rd+start) % BUF_MAX;
	while (len < 2040 && rd != log_wr) {
		uint8_t c = log_buf[rd];
		if (c == '\\' || c == '"') {
			buff[len++] = '\\';
			buff[len++] = c;
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
}

void ICACHE_FLASH_ATTR logInit() {
	log_wr = 0;
	log_rd = 0;
  os_install_putc1((void *)log_write_char);
}


