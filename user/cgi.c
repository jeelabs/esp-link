/*
Some random cgi routines.
*/

#include <string.h>
#include <osapi.h>
#include "httpd.h"
#include "cgi.h"
#include "io.h"

int ICACHE_FLASH_ATTR cgiLed(HttpdConnData *connData) {
	int len;
	char buff[1024];
	
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	len=httpdFindArg(connData->postBuff, "led", buff, sizeof(buff));
	ioLed(atoi(buff));

	httpdRedirect(connData, "led.html");
	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiTest(HttpdConnData *connData) {
	int len;
	char val1[128];
	char val2[128];
	char buff[1024];
	
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Content-Type", "text/plain");
	httpdEndHeaders(connData);

	
	httpdFindArg(connData->postBuff, "Test1", val1, sizeof(val1));
	httpdFindArg(connData->postBuff, "Test2", val2, sizeof(val2));
	len=os_sprintf(buff, "Field 1: %s\nField 2: %s\n", val1, val2);
	espconn_sent(connData->conn, (uint8 *)buff, len);

	return HTTPD_CGI_DONE;
}

