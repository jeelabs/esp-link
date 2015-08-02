/*
Some random cgi routines.
*/

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
#include "cgi.h"
#include "espfs.h"

void ICACHE_FLASH_ATTR
jsonHeader(HttpdConnData *connData, int code) {
	httpdStartResponse(connData, code);
	httpdHeader(connData, "Cache-Control", "no-cache, no-store, must-revalidate");
	httpdHeader(connData, "Pragma", "no-cache");
	httpdHeader(connData, "Expires", "0");
	httpdHeader(connData, "Content-Type", "application/json");
	httpdEndHeaders(connData);
}

#define TOKEN(x) (os_strcmp(token, x) == 0)
#if 0
// Handle system information variables and print their value, returns the number of
// characters appended to buff
int ICACHE_FLASH_ATTR printGlobalInfo(char *buff, int buflen, char *token) {
	if (TOKEN("si_chip_id")) {
		return os_sprintf(buff, "0x%x", system_get_chip_id());
	} else if (TOKEN("si_freeheap")) {
		return os_sprintf(buff, "%dKB", system_get_free_heap_size()/1024);
	} else if (TOKEN("si_uptime")) {
		uint32 t = system_get_time() / 1000000; // in seconds
		return os_sprintf(buff, "%dd%dh%dm%ds", t/(24*3600), (t/(3600))%24, (t/60)%60, t%60);
	} else if (TOKEN("si_boot_version")) {
		return os_sprintf(buff, "%d", system_get_boot_version());
	} else if (TOKEN("si_boot_address")) {
		return os_sprintf(buff, "0x%x", system_get_userbin_addr());
	} else if (TOKEN("si_cpu_freq")) {
		return os_sprintf(buff, "%dMhz", system_get_cpu_freq());
	} else {
		return 0;
	}
}
#endif

extern char *esp_link_version; // in user_main.c

int ICACHE_FLASH_ATTR cgiMenu(HttpdConnData *connData) {
	if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
	char buff[1024];
	// don't use jsonHeader so the response does get cached
	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Cache-Control", "max-age=3600, must-revalidate");
	httpdHeader(connData, "Content-Type", "application/json");
	httpdEndHeaders(connData);
	// construct json response
	os_sprintf(buff,
			"{\"menu\": [\"Home\", \"/home.html\", \"Wifi\", \"/wifi/wifi.html\","
			"\"\xC2\xB5" "C Console\", \"/console.html\", \"Debug log\", \"/log.html\" ],\n"
			" \"version\": \"%s\" }", esp_link_version);
	httpdSend(connData, buff, -1);
	return HTTPD_CGI_DONE;
}
