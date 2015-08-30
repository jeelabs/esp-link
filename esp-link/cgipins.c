
#include <esp8266.h>
#include "cgi.h"
#include "espfs.h"
#include "config.h"
#include "serled.h"
#include "status.h"
#include "serbridge.h"

static char *map_names[] = {
  "esp-bridge", "jn-esp-v2", "esp-01(AVR)", "esp-01(ARM)", "esp-br-rev", "wifi-link-12",
};
static char* map_func[] = { "reset", "isp", "conn_led", "ser_led", "swap_uart" };
static int8_t map_asn[][5] = {
  { 12, 13,  0, 14, 0 },  // esp-bridge
  { 12, 13,  0,  2, 0 },  // jn-esp-v2
  {  0, -1,  2, -1, 0 },  // esp-01(AVR)
  {  0,  2, -1, -1, 0 },  // esp-01(ARM)
  { 13, 12, 14,  0, 0 },  // esp-br-rev -- for test purposes
  {  3,  1,  0,  2, 1 },  // esp-link-12
};
static const int num_map_names = sizeof(map_names)/sizeof(char*);
static const int num_map_func = sizeof(map_func)/sizeof(char*);

// Cgi to return choice of pin assignments
int ICACHE_FLASH_ATTR cgiPinsGet(HttpdConnData *connData) {
	if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted

	char buff[2048];
  int len;

  // figure out current mapping
  int curr = 0;
  for (int i=0; i<num_map_names; i++) {
    int8_t *map = map_asn[i];
    if (map[0] == flashConfig.reset_pin && map[1] == flashConfig.isp_pin &&
        map[2] == flashConfig.conn_led_pin && map[3] == flashConfig.ser_led_pin &&
        map[4] == flashConfig.swap_uart) {
      curr = i;
    }
  }

  // print mapping
  len = os_sprintf(buff, "{ \"curr\":\"%s\", \"map\": [ ", map_names[curr]);
  for (int i=0; i<num_map_names; i++) {
    if (i != 0) buff[len++] = ',';
    len += os_sprintf(buff+len, "\n{ \"value\":%d, \"name\":\"%s\"", i, map_names[i]);
    for (int f=0; f<num_map_func; f++) {
      len += os_sprintf(buff+len, ", \"%s\":%d", map_func[f], map_asn[i][f]);
    }
    len += os_sprintf(buff+len, ", \"descr\":\"");
    for (int f=0; f<num_map_func; f++) {
      int8_t p = map_asn[i][f];
      if (f == 4)
        len += os_sprintf(buff+len, " %s:%s", map_func[f], p?"yes":"no");
      else if (p >= 0)
        len += os_sprintf(buff+len, " %s:gpio%d", map_func[f], p);
      else
        len += os_sprintf(buff+len, " %s:n/a", map_func[f]);
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

  char buff[128];
	int len = httpdFindArg(connData->getArgs, "map", buff, sizeof(buff));
	if (len <= 0) {
	  jsonHeader(connData, 400);
    return HTTPD_CGI_DONE;
  }

  int m = atoi(buff);
	if (m < 0 || m >= num_map_names) {
	  jsonHeader(connData, 400);
    return HTTPD_CGI_DONE;
  }

  os_printf("Switching pin map to %s (%d)\n", map_names[m], m);
  int8_t *map = map_asn[m];
  flashConfig.reset_pin    = map[0];
  flashConfig.isp_pin      = map[1];
  flashConfig.conn_led_pin = map[2];
  flashConfig.ser_led_pin  = map[3];
  flashConfig.swap_uart    = map[4];

  serbridgeInitPins();
  serledInit();
  statusInit();

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

int ICACHE_FLASH_ATTR cgiPins(HttpdConnData *connData) {
	if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
	if (connData->requestType == HTTPD_METHOD_GET) {
		return cgiPinsGet(connData);
	} else if (connData->requestType == HTTPD_METHOD_POST) {
		return cgiPinsSet(connData);
	} else {
		jsonHeader(connData, 404);
		return HTTPD_CGI_DONE;
	}
}
