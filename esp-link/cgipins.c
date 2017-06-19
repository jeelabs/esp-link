
#include <esp8266.h>
#include "cgi.h"
#include "espfs.h"
#include "config.h"
#include "serled.h"
#include "status.h"
#include "serbridge.h"

#if 0
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
  {  1,  3,  0,  2, 1 },  // esp-link-12
};
static const int num_map_names = sizeof(map_names)/sizeof(char*);
static const int num_map_func = sizeof(map_func)/sizeof(char*);
#endif

// Cgi to return choice of pin assignments
int ICACHE_FLASH_ATTR cgiPinsGet(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted

  char buff[1024];
  int len;

  len = os_sprintf(buff,
      "{ \"reset\":%d, \"isp\":%d, \"conn\":%d, \"ser\":%d, \"swap\":%d, \"rxpup\":%d, \"txen\":%d }",
      flashConfig.reset_pin, flashConfig.isp_pin, flashConfig.conn_led_pin,
      flashConfig.ser_led_pin, !!flashConfig.swap_uart, !!flashConfig.rx_pullup,
      flashConfig.tx_enable_pin);

  jsonHeader(connData, 200);
  httpdSend(connData, buff, len);
  return HTTPD_CGI_DONE;
}

// Cgi to change choice of pin assignments
int ICACHE_FLASH_ATTR cgiPinsSet(HttpdConnData *connData) {
  if (connData->conn==NULL) {
    return HTTPD_CGI_DONE; // Connection aborted
  }

  int8_t ok = 0;
  int8_t reset, isp, conn, ser, tx_enable;
  uint8_t swap, rxpup;
  ok |= getInt8Arg(connData, "reset", &reset);
  ok |= getInt8Arg(connData, "isp", &isp);
  ok |= getInt8Arg(connData, "conn", &conn);
  ok |= getInt8Arg(connData, "ser", &ser);
  ok |= getBoolArg(connData, "swap", &swap);
  ok |= getBoolArg(connData, "rxpup", &rxpup);
  ok |= getInt8Arg(connData, "txen", &tx_enable);
  if (ok < 0) return HTTPD_CGI_DONE;

  char *coll;
  if (ok > 0) {
    // check whether two pins collide
    uint16_t pins = 0;
    if (reset >= 0) pins = 1 << reset;
    if (isp >= 0) {
      if (pins & (1<<isp)) { coll = "ISP/Flash"; goto collision; }
      pins |= 1 << isp;
    }
    if (conn >= 0) {
      if (pins & (1<<conn)) { coll = "Conn LED"; goto collision; }
      pins |= 1 << conn;
    }
    if (ser >= 0) {
      if (pins & (1<<ser)) { coll = "Serial LED"; goto collision; }
      pins |= 1 << ser;
    }
    if (tx_enable >= 0) {
        if (pins & (1<<tx_enable)) { coll = "TX Enable"; goto collision; }
        pins |= 1 << tx_enable;
    }
    if (swap) {
      if (pins & (1<<15)) { coll = "Uart TX"; goto collision; }
      if (pins & (1<<13)) { coll = "Uart RX"; goto collision; }
    } else {
      if (pins & (1<<1)) { coll = "Uart TX"; goto collision; }
      if (pins & (1<<3)) { coll = "Uart RX"; goto collision; }
    }

    // we're good, set flashconfig
    flashConfig.reset_pin = reset;
    flashConfig.isp_pin = isp;
    flashConfig.conn_led_pin = conn;
    flashConfig.ser_led_pin = ser;
    flashConfig.swap_uart = swap;
    flashConfig.rx_pullup = rxpup;
    flashConfig.tx_enable_pin = tx_enable;
    os_printf("Pins changed: reset=%d isp=%d conn=%d ser=%d swap=%d rx-pup=%d tx_enable=%d\n",
	reset, isp, conn, ser, swap, rxpup, tx_enable);

    // apply the changes
    serbridgeInitPins();
    serledInit();
    statusInit();

    // save to flash
    if (configSave()) {
      httpdStartResponse(connData, 204);
      httpdEndHeaders(connData);
    } else {
      httpdStartResponse(connData, 500);
      httpdEndHeaders(connData);
      httpdSend(connData, "Failed to save config", -1);
    }
  }
  return HTTPD_CGI_DONE;

collision: {
    char buff[128];
    os_sprintf(buff, "Pin assignment for %s collides with another assignment", coll);
    errorResponse(connData, 400, buff);
    return HTTPD_CGI_DONE;
  }
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
