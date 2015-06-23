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

void ICACHE_FLASH_ATTR printGlobalJSON(HttpdConnData *connData) {
	httpdSend(connData,
			"<script type=\"text/javascript\">\n"
			"var menu = [\"Home\", \"/home.tpl\", \"Wifi\", \"/wifi/wifi.tpl\","
			"\"\xC2\xB5" "C Console\", \"/console.tpl\", \"Debug log\", \"/log.tpl\" ];\n", -1);
#   define VERS_STR_STR(V) #V
#   define VERS_STR(V) VERS_STR_STR(V)
	httpdSend(connData, "version = \"" VERS_STR(VERSION) "\";\n", -1);
  httpdSend(connData, "</script>\n", -1);
}
