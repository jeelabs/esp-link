// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt
// // TCP Client settings

#include <esp8266.h>
#include "cgi.h"
#include "config.h"
#include "cgitcp.h"

// Cgi to return TCP client settings
int ICACHE_FLASH_ATTR cgiTcpGet(HttpdConnData *connData) {
	char buff[1024];
  int len;

	if (connData->conn==NULL) return HTTPD_CGI_DONE;

  len = os_sprintf(buff, "{ \"tcp_enable\":%d, \"rssi_enable\": %d, \"api_key\":\"%s\" }",
			flashConfig.tcp_enable, flashConfig.rssi_enable, flashConfig.api_key);

	jsonHeader(connData, 200);
	httpdSend(connData, buff, len);
	return HTTPD_CGI_DONE;
}

// Cgi to change choice of pin assignments
int ICACHE_FLASH_ATTR cgiTcpSet(HttpdConnData *connData) {
	if (connData->conn==NULL) return HTTPD_CGI_DONE;

	// Handle tcp_enable flag
	char buff[128];
	int len = httpdFindArg(connData->getArgs, "tcp_enable", buff, sizeof(buff));
	if (len <= 0) {
	  jsonHeader(connData, 400);
    return HTTPD_CGI_DONE;
  }
	flashConfig.tcp_enable = os_strcmp(buff, "true") == 0;

	// Handle rssi_enable flag
	len = httpdFindArg(connData->getArgs, "rssi_enable", buff, sizeof(buff));
	if (len <= 0) {
	  jsonHeader(connData, 400);
    return HTTPD_CGI_DONE;
  }
	flashConfig.rssi_enable = os_strcmp(buff, "true") == 0;

	// Handle api_key flag
	len = httpdFindArg(connData->getArgs, "api_key", buff, sizeof(buff));
	if (len < 0) {
	  jsonHeader(connData, 400);
    return HTTPD_CGI_DONE;
  }
	buff[sizeof(flashConfig.api_key)-1] = 0; // ensure we don't get an overrun
	os_strcpy(flashConfig.api_key, buff);

  if (configSave()) {
    httpdStartResponse(connData, 200);
    httpdEndHeaders(connData);
  } else {
    httpdStartResponse(connData, 500);
    httpdEndHeaders(connData);
    httpdSend(connData, "Failed to save config", -1);
  }
	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiTcp(HttpdConnData *connData) {
	if (connData->requestType == HTTPD_METHOD_GET) {
		return cgiTcpGet(connData);
	} else if (connData->requestType == HTTPD_METHOD_POST) {
		return cgiTcpSet(connData);
	} else {
		jsonHeader(connData, 404);
		return HTTPD_CGI_DONE;
	}
}
