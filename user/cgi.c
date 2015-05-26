/*
Some random cgi routines. Used in the LED example and the page that returns the entire
flash as a binary. Also handles the hit counter on the main page.
*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain
 * this notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 */


#include <esp8266.h>
#include "cgi.h"


//cause I can't be bothered to write an ioGetLed()
static char currLedState=0;

//Cgi that turns the LED on or off according to the 'led' param in the POST data
int ICACHE_FLASH_ATTR cgiLed(HttpdConnData *connData) {
	int len;
	char buff[1024];

	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	len=httpdFindArg(connData->post->buff, "led", buff, sizeof(buff));
	if (len!=0) {
		currLedState=atoi(buff);
		//ioLed(currLedState);
	}

	httpdRedirect(connData, "led.tpl");
	return HTTPD_CGI_DONE;
}



//Template code for the led page.
int ICACHE_FLASH_ATTR tplLed(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	if (token==NULL) return HTTPD_CGI_DONE;

	os_strcpy(buff, "Unknown");
	if (os_strcmp(token, "ledstate")==0) {
		if (currLedState) {
			os_strcpy(buff, "on");
		} else {
			os_strcpy(buff, "off");
		}
	} else if (os_strcmp(token, "topnav")==0) {
		printNav(buff);
	}
	httpdSend(connData, buff, -1);
	return HTTPD_CGI_DONE;
}

static long hitCounter=0;

//Template code for the counter on the index page.
int ICACHE_FLASH_ATTR tplCounter(HttpdConnData *connData, char *token, void **arg) {
	char buff[256];
	if (token==NULL) return HTTPD_CGI_DONE;

	if (printSysInfo(buff, token) > 0) {
		// awesome...
	} else if (os_strcmp(token, "topnav")==0) {
		printNav(buff);
	} else if (os_strcmp(token, "counter")==0) {
		hitCounter++;
		os_sprintf(buff, "%ld", hitCounter);
	}
	httpdSend(connData, buff, -1);
	return HTTPD_CGI_DONE;
}

static char *navLinks[][2] = {
	{ "Home", "/index.tpl" }, { "Wifi", "/wifi/wifi.tpl" }, { "\xC2\xB5""C Console", "/console.tpl" },
	{ "Esp log", "/log.tpl" }, { "Help", "/help.tpl" },
	{ 0, 0 },
};

// Print the navigation links into the buffer and return the length of what got added
int ICACHE_FLASH_ATTR printNav(char *buff) {
	int len = 0;
	for (uint8_t i=0; navLinks[i][0] != NULL; i++) {
		if (i > 0) buff[len++] = '|';
		//os_printf("nav %d: %s -> %s\n", i, navLinks[i][0], navLinks[i][1]);
		len += os_sprintf(buff+len, " <a href=\"%s\">%s</a> ", navLinks[i][1], navLinks[i][0]);
	}
	//os_printf("nav: %s\n", buff);
	return len;
}

#define TOKEN(x) (os_strcmp(token, x) == 0)

// Handle system information variables and print their value, returns the number of
// characters appended to buff
int ICACHE_FLASH_ATTR printSysInfo(char *buff, char *token) {
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

