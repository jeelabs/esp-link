#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "httpd.h"
#include "io.h"

extern uint8_t at_wifiMode;

void user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	httpdInit();
	ioInit();
	os_printf("\nReady\n");
}
