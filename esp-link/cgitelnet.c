#include <esp8266.h>
#include "cgi.h"
#include "config.h"
#include "serbridge.h"

// Cgi to return choice of Telnet ports
int ICACHE_FLASH_ATTR cgiTelnetGet(HttpdConnData *connData) {
  char buff[80];

  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted

  int len;
  
  os_printf("Current telnet ports: port0=%d port1=%d\n",
	flashConfig.telnet_port0, flashConfig.telnet_port1);
	
  len = os_sprintf(buff,
      "{ \"port0\": \"%d\", \"port1\": \"%d\" }",
      flashConfig.telnet_port0, flashConfig.telnet_port1);

  jsonHeader(connData, 200);
  httpdSend(connData, buff, len);

  return HTTPD_CGI_DONE;
}

// Cgi to change choice of Telnet ports
int ICACHE_FLASH_ATTR cgiTelnetSet(HttpdConnData *connData) {
  char buf[80];

  if (connData->conn==NULL) {
    return HTTPD_CGI_DONE; // Connection aborted
  }

  int8_t ok = 0;
  uint16_t port0, port1;
  ok |= getUInt16Arg(connData, "port0", &port0);
  ok |= getUInt16Arg(connData, "port1", &port1);

  if (ok <= 0) { //If we get at least one good value, this should be >= 1
    ets_sprintf(buf, "Unable to fetch telnet ports.\n Received: port0=%d port1=%d\n",
	  flashConfig.telnet_port0, flashConfig.telnet_port1);
    os_printf(buf);
    errorResponse(connData, 400, buf);
    return HTTPD_CGI_DONE;
  }

  if (ok > 0) {
    // fill both port variables from flash or ajax provided value
    if (!port0) port0 = flashConfig.telnet_port0;
    if (!port1) port1 = flashConfig.telnet_port1;
  
    // check whether ports are different
    if (port0 == port1) {
      os_sprintf(buf, "Ports cannot be the same.\n Tried to set: port0=%d port1=%d\n",
        flashConfig.telnet_port0, flashConfig.telnet_port1);
      os_printf(buf);
      errorResponse(connData, 400, buf);
      return HTTPD_CGI_DONE;
    }

    // we're good, set flashconfig
    flashConfig.telnet_port0 = port0;
    flashConfig.telnet_port1 = port1;
    os_printf("Telnet ports changed: port0=%d port1=%d\n",
	  flashConfig.telnet_port0, flashConfig.telnet_port1);

    // save to flash
    if (configSave()) {
      httpdStartResponse(connData, 204);
      httpdEndHeaders(connData);
    } else {
      httpdStartResponse(connData, 500);
      httpdEndHeaders(connData);
      httpdSend(connData, "Failed to save config", -1);
    }
    
    // apply the changes
    serbridgeInit();
    serbridgeStart(0, flashConfig.telnet_port0, flashDefault.telnet_port0mode);
    serbridgeStart(1, flashConfig.telnet_port1, flashDefault.telnet_port1mode);
  
  }

  return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiTelnet(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  if (connData->requestType == HTTPD_METHOD_GET) {
    return cgiTelnetGet(connData);
  } else if (connData->requestType == HTTPD_METHOD_POST) {
    return cgiTelnetSet(connData);
  } else {
    jsonHeader(connData, 404);
    return HTTPD_CGI_DONE;
  }
}
