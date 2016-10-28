#include <esp8266.h>
#include "cgi.h"
#include "config.h"
#include "serbridge.h"

// Cgi to return choice of Telnet ports
int ICACHE_FLASH_ATTR cgiTelnetGet(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted

  char buff[1024];
  int len;

  len = os_sprintf(buff,
      "{ \"telnet-port1\":%d, \"telnet-port2\":%d }",
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
  if (ok < 0) { coll = "Failed to set ports. Ports appear to be invalid"; goto collision; }

  char *coll;
  if (ok > 0) {
    // fill both port variables from flash or ajax provided value
    if (!port1) port1 = flashConfig.telnet_port1;
    if (!port2) port2 = flashConfig.telnet_port2;
  
    // check whether ports are different
    if (port1 == port2) { coll = "Ports cannot be the same!"; goto collision; }
  
    // we're good, set flashconfig
    flashConfig.telnet_port1 = port1;
    flashConfig.telnet_port2 = port2;
    os_printf("Ports changed: port1=%d port2=%d\n",
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
    serbridgeInit(flashConfig.telnet_port1, flashConfig.telnet_port2);
  }
  return HTTPD_CGI_DONE;

 collision: {
    char buff[128];
    os_sprintf(buff, "Ports assignment for %s collides with another assignment", coll);
    errorResponse(connData, 400, buff);
    return HTTPD_CGI_DONE;
  }
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
