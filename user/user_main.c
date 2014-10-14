#include "espmissingincludes.h"
#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "httpd.h"
#include "io.h"
#include "httpdespfs.h"
#include "cgi.h"

HttpdBuiltInUrl builtInUrls[]={
	{"/", cgiRedirect, "/test2.html"},
	{"/flash.bin", cgiReadFlash, NULL},
	{"/led.cgi", cgiLed, NULL},

	{"/wifi", cgiRedirect, "/wifi/wifi.tpl"},
	{"/wifi/", cgiRedirect, "/wifi/wifi.tpl"},
	{"/wifi/wifiscan.cgi", cgiWiFiScan, NULL},
	{"/wifi/wifi.tpl", cgiEspFsTemplate, tplWlan},
	{"/wifi/connect.cgi", cgiWiFiConnect},

	{"*", cgiEspFsHook, NULL},
	{NULL, NULL, NULL}
};


void user_init(void) {
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	ioInit();
	httpdInit(builtInUrls, 80);
	os_printf("\nReady\n");
}
