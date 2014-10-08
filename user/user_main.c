#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "httpd.h"
#include "io.h"
#include "httpdespfs.h"

HttpdBuiltInUrl builtInUrls[]={
//	{"/", cgiLiteral, "Lalala etc"},
	{"*", cgiEspFsHook, NULL},
	{NULL, NULL, NULL}
};


void user_init(void) {
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	ioInit();
	httpdInit(builtInUrls, 80);
	os_printf("\nReady\n");
}
