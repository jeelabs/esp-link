// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt
//
// Adapted from: github.com/tuanpmt/esp_bridge, Created on: Jan 9, 2015, Author: Minh

#include "esp8266.h"
#include "sntp.h"
#include "cmd.h"
#include "uart.h"
#include <cgiwifi.h>
#ifdef MQTT
#include <mqtt_cmd.h>
#endif
#ifdef REST
#include <rest.h>
#endif
#include <web-server.h>
#ifdef SOCKET
#include <socket.h>
#endif

#ifdef CMD_DBG
#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)
#else
#define DBG(format, ...) do { } while(0)
#endif

static void cmdNull(CmdPacket *cmd);
static void cmdSync(CmdPacket *cmd);
static void cmdWifiStatus(CmdPacket *cmd);
static void cmdGetTime(CmdPacket *cmd);
static void cmdAddCallback(CmdPacket *cmd);

// keep track of last status sent to uC so we can notify it when it changes
static uint8_t lastWifiStatus = wifiIsDisconnected;
// keep track of whether we have registered our cb handler with the wifi subsystem
static bool wifiCbAdded = false;
// keep track of whether we received a sync command from uC
bool cmdInSync = false;

// Command dispatch table for serial -> ESP commands
const CmdList commands[] = {
  {CMD_NULL,            "NULL",           cmdNull},        // no-op
  {CMD_SYNC,            "SYNC",           cmdSync},        // synchronize
  {CMD_WIFI_STATUS,     "WIFI_STATUS",    cmdWifiStatus},
  {CMD_CB_ADD,          "ADD_CB",         cmdAddCallback},
  {CMD_GET_TIME,        "GET_TIME",       cmdGetTime},
#ifdef MQTT
  {CMD_MQTT_SETUP,      "MQTT_SETUP",     MQTTCMD_Setup},
  {CMD_MQTT_PUBLISH,    "MQTT_PUB",       MQTTCMD_Publish},
  {CMD_MQTT_SUBSCRIBE , "MQTT_SUB",       MQTTCMD_Subscribe},
  {CMD_MQTT_LWT,        "MQTT_LWT",       MQTTCMD_Lwt},
#endif
#ifdef REST
  {CMD_REST_SETUP,      "REST_SETUP",     REST_Setup},
  {CMD_REST_REQUEST,    "REST_REQ",       REST_Request},
  {CMD_REST_SETHEADER,  "REST_SETHDR",    REST_SetHeader},
#endif
  {CMD_WEB_SETUP,       "WEB_SETUP",      WEB_Setup},
  {CMD_WEB_DATA,        "WEB_DATA",       WEB_Data},
#ifdef SOCKET
  {CMD_SOCKET_SETUP,    "SOCKET_SETUP",   SOCKET_Setup},
  {CMD_SOCKET_SEND,     "SOCKET_SEND",    SOCKET_Send},
#endif
};

//===== List of registered callbacks (to uC)

// WifiCb plus 10 for other stuff
#define MAX_CALLBACKS 12
CmdCallback callbacks[MAX_CALLBACKS]; // cleared in cmdSync

uint32_t ICACHE_FLASH_ATTR
cmdAddCb(char* name, uint32_t cb) {
  for (uint8_t i = 0; i < MAX_CALLBACKS; i++) {
    //os_printf("cmdAddCb: index %d name=%s cb=%p\n", i, callbacks[i].name,
    //  (void *)callbacks[i].callback);
    // find existing callback or add to the end
    if (os_strncmp(callbacks[i].name, name, CMD_CBNLEN) == 0 || callbacks[i].name[0] == '\0') {
      os_strncpy(callbacks[i].name, name, sizeof(callbacks[i].name));
      callbacks[i].name[CMD_CBNLEN-1] = 0; // strncpy doesn't null terminate
      callbacks[i].callback = cb;
      DBG("cmdAddCb: '%s'->0x%x added at %d\n", callbacks[i].name, cb, i);
      return 1;
    }
  }
  return 0;
}

CmdCallback* ICACHE_FLASH_ATTR
cmdGetCbByName(char* name) {
  for (uint8_t i = 0; i < MAX_CALLBACKS; i++) {
    //os_printf("cmdGetCbByName: index %d name=%s cb=%p\n", i, callbacks[i].name,
    //  (void *)callbacks[i].callback);
    // if callback doesn't exist or it's null
    if (os_strncmp(callbacks[i].name, name, CMD_CBNLEN) == 0) {
      DBG("cmdGetCbByName: cb %s found at index %d\n", name, i);
      return &callbacks[i];
    }
  }
  os_printf("cmdGetCbByName: cb %s not found\n", name);
  return 0;
}

//===== Wifi callback

// Callback from wifi subsystem to notify us of status changes
static void ICACHE_FLASH_ATTR
cmdWifiCb(uint8_t wifiStatus) {
  if (wifiStatus != lastWifiStatus){
    DBG("cmdWifiCb: wifiStatus=%d\n", wifiStatus);
    lastWifiStatus = wifiStatus;
    CmdCallback *wifiCb = cmdGetCbByName("wifiCb");
    if ((uint32_t)wifiCb->callback != -1) {
      uint8_t status = wifiStatus == wifiGotIP ? 5 : 1;
      cmdResponseStart(CMD_RESP_CB, (uint32_t)wifiCb->callback, 1);
      cmdResponseBody((uint8_t*)&status, 1);
      cmdResponseEnd();
    }
  }
}

//===== Command handlers

// Command handler for Null command
static void ICACHE_FLASH_ATTR
cmdNull(CmdPacket *cmd) {
}

// Command handler for sync command
static void ICACHE_FLASH_ATTR
cmdSync(CmdPacket *cmd) {
  CmdRequest req;
  uart0_write_char(SLIP_END); // prefix with a SLIP END to ensure we get a clean start
  cmdRequest(&req, cmd);
  if(cmd->argc != 0 || cmd->value == 0) {
    cmdResponseStart(CMD_RESP_V, 0, 0);
    cmdResponseEnd();
    return;
  }

  // clear callbacks table
  os_memset(callbacks, 0, sizeof(callbacks));

  // TODO: call other protocols back to tell them to reset

  // register our callback with wifi subsystem
  if (!wifiCbAdded) {
    wifiAddStateChangeCb(cmdWifiCb);
    wifiCbAdded = true;
  }

  // send OK response
  cmdResponseStart(CMD_RESP_V, cmd->value, 0);
  cmdResponseEnd();
  cmdInSync = true;

  // save the MCU's callback and trigger an initial callback
  cmdAddCb("wifiCb", cmd->value);
  lastWifiStatus = 0xff; // set to invalid value so we immediately send status cb in all cases
  cmdWifiCb(wifiState);

  return;
}

// Command handler for wifi status command
static void ICACHE_FLASH_ATTR
cmdWifiStatus(CmdPacket *cmd) {
  cmdResponseStart(CMD_RESP_V, wifiState, 0);
  cmdResponseEnd();
  return;
}

// Command handler for time
static void ICACHE_FLASH_ATTR
cmdGetTime(CmdPacket *cmd) {
  cmdResponseStart(CMD_RESP_V, sntp_get_current_timestamp(), 0);
  cmdResponseEnd();
  return;
}

// Command handler to add a callback to the named-callbacks list, this is for a callback to the uC
static void ICACHE_FLASH_ATTR
cmdAddCallback(CmdPacket *cmd) {
  CmdRequest req;
  cmdRequest(&req, cmd);
  if (cmd->argc != 1 || cmd->value == 0) return;

  char name[16];
  uint16_t len;

  // get the callback name
  len = cmdArgLen(&req);
  if (len > 15) return; // max size of name is 15 characters
  if (cmdPopArg(&req, (uint8_t *)name, len)) return;
  name[len] = 0;
  DBG("cmdAddCallback: name=%s\n", name);

  cmdAddCb(name, cmd->value); // save the sensor callback
}
