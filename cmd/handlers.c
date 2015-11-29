// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt
//
// Adapted from: github.com/tuanpmt/esp_bridge, Created on: Jan 9, 2015, Author: Minh

#include "esp8266.h"
#include "cmd.h"
#include <cgiwifi.h>
#ifdef MQTT
#include <mqtt_cmd.h>
#endif
#ifdef REST
#include <rest.h>
#endif

#ifdef CMD_DBG
#define DBG(format, ...) os_printf(format, ## __VA_ARGS__)
#else
#define DBG(format, ...) do { } while(0)
#endif

static uint32_t CMD_Null(CmdPacket *cmd);
static uint32_t CMD_IsReady(CmdPacket *cmd);
static uint32_t CMD_Reset(CmdPacket *cmd);
static uint32_t CMD_WifiConnect(CmdPacket *cmd);
static uint32_t CMD_AddCallback(CmdPacket *cmd);

// keep track of last status sent to uC so we can notify it when it changes
static uint8_t lastWifiStatus = wifiIsDisconnected;
static bool wifiCbAdded = false;

// Command dispatch table for serial -> ESP commands
const CmdList commands[] = {
  {CMD_NULL,            CMD_Null},
  {CMD_RESET,           CMD_Reset},
  {CMD_IS_READY,        CMD_IsReady},
  {CMD_WIFI_CONNECT,    CMD_WifiConnect},
#ifdef MQTT
  {CMD_MQTT_SETUP,      MQTTCMD_Setup},
  {CMD_MQTT_CONNECT,    MQTTCMD_Connect},
  {CMD_MQTT_DISCONNECT, MQTTCMD_Disconnect},
  {CMD_MQTT_PUBLISH,    MQTTCMD_Publish},
  {CMD_MQTT_SUBSCRIBE , MQTTCMD_Subscribe},
  {CMD_MQTT_LWT,        MQTTCMD_Lwt},
#endif
#ifdef REST
  {CMD_REST_SETUP,      REST_Setup},
  {CMD_REST_REQUEST,    REST_Request},
  {CMD_REST_SETHEADER,  REST_SetHeader},
#endif
  {CMD_CB_ADD,          CMD_AddCallback},
  {CMD_NULL,            NULL}
};

// WifiCb plus 10 for sensors
#define MAX_CALLBACKS 12
cmdCallback callbacks[MAX_CALLBACKS]; // cleared in CMD_Reset

// Command handler for IsReady (healthcheck) command
static uint32_t ICACHE_FLASH_ATTR
CMD_IsReady(CmdPacket *cmd) {
  return 1;
}

// Command handler for Null command
static uint32_t ICACHE_FLASH_ATTR
CMD_Null(CmdPacket *cmd) {
  return 1;
}

// Command handler for Reset command, this was originally to reset the ESP but we don't want to
// do that is esp-link. It is still good to clear any information the ESP has about the attached
// uC.
static uint32_t ICACHE_FLASH_ATTR
CMD_Reset(CmdPacket *cmd) {
  // clear callbacks table
  os_memset(callbacks, 0, sizeof(callbacks));
  return 1;
}

static uint32_t ICACHE_FLASH_ATTR
CMD_AddCb(char* name, uint32_t cb) {
  for (uint8_t i = 0; i < MAX_CALLBACKS; i++) {
    //os_printf("CMD_AddCb: index %d name=%s cb=%p\n", i, callbacks[i].name,
    //  (void *)callbacks[i].callback);
    // find existing callback or add to the end
    if (os_strncmp(callbacks[i].name, name, CMD_CBNLEN) == 0 || callbacks[i].name[0] == '\0') {
      os_strncpy(callbacks[i].name, name, sizeof(callbacks[i].name));
      callbacks[i].name[CMD_CBNLEN-1] = 0; // strncpy doesn't null terminate
      callbacks[i].callback = cb;
      DBG("CMD_AddCb: cb %s added at index %d\n", callbacks[i].name, i);
      return 1;
    }
  }
  return 0;
}

cmdCallback* ICACHE_FLASH_ATTR
CMD_GetCbByName(char* name) {
  for (uint8_t i = 0; i < MAX_CALLBACKS; i++) {
    //os_printf("CMD_GetCbByName: index %d name=%s cb=%p\n", i, callbacks[i].name,
    //  (void *)callbacks[i].callback);
    // if callback doesn't exist or it's null
    if (os_strncmp(callbacks[i].name, name, CMD_CBNLEN) == 0) {
      DBG("CMD_GetCbByName: cb %s found at index %d\n", name, i);
      return &callbacks[i];
    }
  }
  os_printf("CMD_GetCbByName: cb %s not found\n", name);
  return 0;
}

// Callback from wifi subsystem to notify us of status changes
static void ICACHE_FLASH_ATTR
CMD_WifiCb(uint8_t wifiStatus) {
  if (wifiStatus != lastWifiStatus){
    DBG("CMD_WifiCb: wifiStatus=%d\n", wifiStatus);
    lastWifiStatus = wifiStatus;
    cmdCallback *wifiCb = CMD_GetCbByName("wifiCb");
    if ((uint32_t)wifiCb->callback != -1) {
      uint8_t status = wifiStatus == wifiGotIP ? 5 : 1;
      uint16_t crc = CMD_ResponseStart(CMD_WIFI_CONNECT, (uint32_t)wifiCb->callback, 0, 1);
      crc = CMD_ResponseBody(crc, (uint8_t*)&status, 1);
      CMD_ResponseEnd(crc);
    }
  }
}

// Command handler for Wifi connect command
static uint32_t ICACHE_FLASH_ATTR
CMD_WifiConnect(CmdPacket *cmd) {
  CmdRequest req;
  CMD_Request(&req, cmd);
	if(cmd->argc != 2 || cmd->callback == 0)
		return 0;

  if (!wifiCbAdded) {
    wifiAddStateChangeCb(CMD_WifiCb);    // register our callback with wifi subsystem
    wifiCbAdded = true;
  }
  CMD_AddCb("wifiCb", (uint32_t)cmd->callback); // save the MCU's callback
  lastWifiStatus = 0xff; // set to invalid value so we immediately send status cb in all cases
  CMD_WifiCb(wifiState);

  return 1;
}

// Command handler to add a callback to the named-callbacks list, this is for a callback to the uC
static uint32_t ICACHE_FLASH_ATTR
CMD_AddCallback(CmdPacket *cmd) {
  CmdRequest req;
  CMD_Request(&req, cmd);
  if (cmd->argc != 1 || cmd->callback == 0)
    return 0;

  char name[16];
  uint16_t len;

  // get the sensor name
  len = CMD_ArgLen(&req);
  if (len > 15) return 0; // max size of name is 15 characters
  if (CMD_PopArg(&req, (uint8_t *)name, len)) return 0;
  name[len] = 0;
  DBG("CMD_AddCallback: name=%s\n", name);

  return CMD_AddCb(name, (uint32_t)cmd->callback); // save the sensor callback
}
