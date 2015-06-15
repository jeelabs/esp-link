
#include <esp8266.h>
#include "cgi.h"
#include "espfs.h"

static char *map_names[] = {
  "esp-bridge", "jn-esp-v2", "esp-01"
};
static char* map_func[] = { "reset", "isp", "conn_led", "ser_led" };
static uint8_t map_asn[][4] = {
  { 12, 13,  0, 14 },  // esp-bridge
  { 12, 13,  0,  2 },  // jn-esp-v2
  {  0,  2, 12, 13 },  // esp-01
};

// Cgi to return choice of pin assignments
int ICACHE_FLASH_ATTR cgiPinsGet(HttpdConnData *connData) {
	char buff[1024];
  int len;

	if (connData->conn==NULL) {
		return HTTPD_CGI_DONE; // Connection aborted
	}

  len = os_sprintf(buff, "{ \"curr\":\"esp-bridge\", \"map\": [ ");
  for (int i=0; i<sizeof(map_names)/sizeof(char*); i++) {
    if (i != 0) buff[len++] = ',';
    len += os_sprintf(buff+len, "\n{ \"value\":%d, \"name\":\"%s\"", i, map_names[i]);
    for (int f=0; f<sizeof(map_func)/sizeof(char*); f++) {
      len += os_sprintf(buff+len, ", \"%s\":%d", map_func[f], map_asn[i][f]);
    }
    len += os_sprintf(buff+len, ", \"descr\":\"");
    for (int f=0; f<sizeof(map_func)/sizeof(char*); f++) {
      len += os_sprintf(buff+len, " %s:gpio%d", map_func[f], map_asn[i][f]);
    }
    len += os_sprintf(buff+len, "\" }");
  }
  len += os_sprintf(buff+len, "\n] }");

	jsonHeader(connData, 200);
	httpdSend(connData, buff, len);
	return HTTPD_CGI_DONE;
}

// Cgi to change choice of pin assignments
int ICACHE_FLASH_ATTR cgiPinsSet(HttpdConnData *connData) {
	if (connData->conn==NULL) {
		return HTTPD_CGI_DONE; // Connection aborted
	}

	jsonHeader(connData, 200);
	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiPins(HttpdConnData *connData) {
	if (connData->requestType == HTTPD_METHOD_GET) {
		return cgiPinsGet(connData);
	} else if (connData->requestType == HTTPD_METHOD_POST) {
		return cgiPinsSet(connData);
	} else {
		jsonHeader(connData, 404);
		return HTTPD_CGI_DONE;
	}
}
