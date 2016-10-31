#include <esp8266.h>
#include "cgi.h"
#include "config.h"
#include "serbridge.h"

// Cgi to return choice of Telnet ports
int ICACHE_FLASH_ATTR cgiTelnetGet(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted

  char buff[1024];
  int len;
  
  os_printf("Current telnet ports: port1=%d port2=%d\n",
	flashConfig.telnet_port1, flashConfig.telnet_port2);
	
  len = os_sprintf(buff,
      "{ \"port1\": \"%d\", \"port2\": \"%d\" }",
      flashConfig.telnet_port1, flashConfig.telnet_port2);

  jsonHeader(connData, 200);
  httpdSend(connData, buff, len);
  return HTTPD_CGI_DONE;
}

// Cgi to change choice of Telnet ports
int ICACHE_FLASH_ATTR cgiTelnetSet(HttpdConnData *connData) {
  if (connData->conn==NULL) {
    return HTTPD_CGI_DONE; // Connection aborted
  }
	
  int8_t ok = 0;
  uint16_t port1, port2;
  ok |= getUInt16Arg(connData, "port1", &port1);
  ok |= getUInt16Arg(connData, "port2", &port2);
  if (ok <= 0) { //If we get at least one good value, this should be >= 1
    os_printf("Unable to fetch telnet ports.\n Received: port1=%d port2=%d\n",
	  flashConfig.telnet_port1, flashConfig.telnet_port2);
    errorResponse(connData, 400, buff);
    return HTTPD_CGI_DONE;
  }

  if (ok > 0) {
    // fill both port variables from flash or ajax provided value
    if (!port1) port1 = flashConfig.telnet_port1;
    if (!port2) port2 = flashConfig.telnet_port2;
  
    // check whether ports are different
    if (port1 == port2) {
      os_printf("Ports cannot be the same.\n Tried to set: port1=%d port2=%d\n",
      flashConfig.telnet_port1, flashConfig.telnet_port2);
      errorResponse(connData, 400, buff);
      return HTTPD_CGI_DONE;
    }
  
    // we're good, set flashconfig
    flashConfig.telnet_port1 = port1;
    flashConfig.telnet_port2 = port2;
    os_printf("Telnet ports changed: port1=%d port2=%d\n",
	  flashConfig.telnet_port1, flashConfig.telnet_port2);

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
    serbridgeStart(1, flashConfig.telnet_port1, flashDefault.telnet_port1mode);
    serbridgeStart(2, flashConfig.telnet_port2, flashDefault.telnet_port2mode);
  
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
