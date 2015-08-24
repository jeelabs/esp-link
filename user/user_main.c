/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain
 * this notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 * Heavily modified and enhanced by Thorsten von Eicken in 2015
 * ----------------------------------------------------------------------------
 */


#include <esp8266.h>
#include "httpd.h"
#include "httpdespfs.h"
#include "cgi.h"
#include "cgiwifi.h"
#include "cgipins.h"
#include "cgitcp.h"
#include "cgiflash.h"
#include "auth.h"
#include "espfs.h"
#include "uart.h"
#include "serbridge.h"
#include "status.h"
#include "serled.h"
#include "console.h"
#include "config.h"
#include "log.h"
#include <gpio.h>

//#define SHOW_HEAP_USE

//Function that tells the authentication system what users/passwords live on the system.
//This is disabled in the default build; if you want to try it, enable the authBasic line in
//the builtInUrls below.
int myPassFn(HttpdConnData *connData, int no, char *user, int userLen, char *pass, int passLen) {
	if (no==0) {
		os_strcpy(user, "admin");
		os_strcpy(pass, "s3cr3t");
		return 1;
//Add more users this way. Check against incrementing no for each user added.
//	} else if (no==1) {
//		os_strcpy(user, "user1");
//		os_strcpy(pass, "something");
//		return 1;
	}
	return 0;
}


/*
This is the main url->function dispatching data struct.
In short, it's a struct with various URLs plus their handlers. The handlers can
be 'standard' CGI functions you wrote, or 'special' CGIs requiring an argument.
They can also be auth-functions. An asterisk will match any url starting with
everything before the asterisks; "*" matches everything. The list will be
handled top-down, so make sure to put more specific rules above the more
general ones. Authorization things (like authBasic) act as a 'barrier' and
should be placed above the URLs they protect.
*/
HttpdBuiltInUrl builtInUrls[]={
	{"/", cgiRedirect, "/home.html"},
	{"/menu", cgiMenu, NULL},
	{"/flash/next", cgiGetFirmwareNext, NULL},
	{"/flash/upload", cgiUploadFirmware, NULL},
	{"/flash/reboot", cgiRebootFirmware, NULL},
	//{"/home.html", cgiEspFsHtml, NULL},
	//{"/log.html", cgiEspFsHtml, NULL},
	{"/log/text", ajaxLog, NULL},
	{"/log/dbg", ajaxLogDbg, NULL},
	//{"/console.html", cgiEspFsHtml, NULL},
	{"/console/reset", ajaxConsoleReset, NULL},
	{"/console/baud", ajaxConsoleBaud, NULL},
	{"/console/text", ajaxConsole, NULL},

	//Routines to make the /wifi URL and everything beneath it work.

//Enable the line below to protect the WiFi configuration with an username/password combo.
//	{"/wifi/*", authBasic, myPassFn},

	{"/wifi", cgiRedirect, "/wifi/wifi.html"},
	{"/wifi/", cgiRedirect, "/wifi/wifi.html"},
	//{"/wifi/wifi.html", cgiEspFsHtml, NULL},
	{"/wifi/info", cgiWifiInfo, NULL},
	{"/wifi/scan", cgiWiFiScan, NULL},
	{"/wifi/connect", cgiWiFiConnect, NULL},
	{"/wifi/connstatus", cgiWiFiConnStatus, NULL},
	{"/wifi/setmode", cgiWiFiSetMode, NULL},
	{"/wifi/special", cgiWiFiSpecial, NULL},
	{"/pins", cgiPins, NULL},
	{"/tcpclient", cgiTcp, NULL},

	{"*", cgiEspFsHook, NULL}, //Catch-all cgi function for the filesystem
	{NULL, NULL, NULL}
};


//#define SHOW_HEAP_USE

#ifdef SHOW_HEAP_USE
static ETSTimer prHeapTimer;

static void ICACHE_FLASH_ATTR prHeapTimerCb(void *arg) {
	os_printf("Heap: %ld\n", (unsigned long)system_get_free_heap_size());
}
#endif

void user_rf_pre_init(void) {
}

// address of espfs binary blob
extern uint32_t _binary_espfs_img_start;

static char *rst_codes[] = {
	"normal", "wdt reset", "exception", "soft wdt", "restart", "deep sleep", "external",
};

# define VERS_STR_STR(V) #V
# define VERS_STR(V) VERS_STR_STR(V)
char *esp_link_version = VERS_STR(VERSION);

//Main routine. Initialize stdout, the I/O, filesystem and the webserver and we're done.
void user_init(void) {
	// get the flash config so we know how to init things
	//configWipe(); // uncomment to reset the config for testing purposes
	bool restoreOk = configRestore();
	// init gpio pin registers
	gpio_init();
	// init UART
	uart_init(flashConfig.baud_rate, 115200);
	logInit(); // must come after init of uart
	// say hello (leave some time to cause break in TX after boot loader's msg
	os_delay_us(10000L);
	os_printf("\n\n** %s\n", esp_link_version);
	os_printf("Flash config restore %s\n", restoreOk ? "ok" : "*FAILED*");
	// Status LEDs
	statusInit();
	serledInit();
	// Wifi
	wifiInit();
	// init the flash filesystem with the html stuff
	espFsInit(&_binary_espfs_img_start);
	//EspFsInitResult res = espFsInit(&_binary_espfs_img_start);
	//os_printf("espFsInit %s\n", res?"ERR":"ok");
	// mount the http handlers
	httpdInit(builtInUrls, 80);
	// init the wifi-serial transparent bridge (port 23)
	serbridgeInit(23);
	uart_add_recv_cb(&serbridgeUartCb);
#ifdef SHOW_HEAP_USE
	os_timer_disarm(&prHeapTimer);
	os_timer_setfn(&prHeapTimer, prHeapTimerCb, NULL);
	os_timer_arm(&prHeapTimer, 10000, 1);
#endif

	struct rst_info *rst_info = system_get_rst_info();
	os_printf("Reset cause: %d=%s\n", rst_info->reason, rst_codes[rst_info->reason]);
	os_printf("exccause=%d epc1=0x%x epc2=0x%x epc3=0x%x excvaddr=0x%x depc=0x%x\n",
			rst_info->exccause, rst_info->epc1, rst_info->epc2, rst_info->epc3,
			rst_info->excvaddr, rst_info->depc);
	os_printf("Flash map %d, chip %08X\n", system_get_flash_size_map(), spi_flash_get_id());

	os_printf("** esp-link ready\n");
}
