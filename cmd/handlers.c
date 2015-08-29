// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt
//
// Adapted from: github.com/tuanpmt/esp_bridge, Created on: Jan 9, 2015, Author: Minh

#include "esp8266.h"
#include "cmd.h"
#include "rest.h"
#include "crc16.h"
#include "serbridge.h"
#include "uart.h"
#include "cgiwifi.h"
#include "mqtt_cmd.h"

static uint32_t ICACHE_FLASH_ATTR CMD_Null(CmdPacket *cmd);
static uint32_t ICACHE_FLASH_ATTR CMD_IsReady(CmdPacket *cmd);
static uint32_t ICACHE_FLASH_ATTR CMD_WifiConnect(CmdPacket *cmd);
static uint32_t ICACHE_FLASH_ATTR CMD_AddSensor(CmdPacket *cmd);

static uint8_t lastWifiStatus = wifiIsDisconnected;

// Command dispatch table for serial -> ESP commands
const CmdList commands[] = {
  {CMD_NULL,            CMD_Null},
  {CMD_RESET,           CMD_Null},
  {CMD_IS_READY,        CMD_IsReady},
  {CMD_WIFI_CONNECT,    CMD_WifiConnect},

  {CMD_MQTT_SETUP,      MQTTCMD_Setup},
  {CMD_MQTT_CONNECT,    MQTTCMD_Connect},
  {CMD_MQTT_DISCONNECT, MQTTCMD_Disconnect},
  {CMD_MQTT_PUBLISH,    MQTTCMD_Publish},
  {CMD_MQTT_SUBSCRIBE , MQTTCMD_Subscribe},
  {CMD_MQTT_LWT,        MQTTCMD_Lwt},  

  {CMD_REST_SETUP,      REST_Setup},
  {CMD_REST_REQUEST,    REST_Request},
  {CMD_REST_SETHEADER,  REST_SetHeader},

  {CMD_ADD_SENSOR,      CMD_AddSensor },

  {CMD_NULL,            NULL}
};

// WifiCb plus 10 for sensors
cmdCallback callbacks[12] = {
  { { '\0' }, -1 },
  { { '\0' }, -1 },
  { { '\0' }, -1 },
  { { '\0' }, -1 },
  { { '\0' }, -1 },
  { { '\0' }, -1 },
  { { '\0' }, -1 },
  { { '\0' }, -1 },
  { { '\0' }, -1 },
  { { '\0' }, -1 },
  { { '\0' }, -1 }
};

// Command handler for IsReady (healthcheck) command
static uint32_t ICACHE_FLASH_ATTR
CMD_IsReady(CmdPacket *cmd) {
  os_printf("CMD_IsReady: Check ready\n");
  return 1;
}

// Command handler for Null command
static uint32_t ICACHE_FLASH_ATTR
CMD_Null(CmdPacket *cmd) {
  os_printf("CMD_Null: NULL/unsupported command\n");
  return 1;
}

static void ICACHE_FLASH_ATTR 
CMD_AddCb(char* name, uint32_t cb) {
  char checkname[16];
  os_strncpy(checkname, name, sizeof(checkname));
  for (uint8_t i = 0; i < sizeof(commands); i++) {    
    os_printf("CMD_AddCb: index %d name=%s cb=%p\n", i, callbacks[i].name, (void *)callbacks[i].callback);
    // find existing callback or add to the end
    if (os_strcmp(callbacks[i].name, checkname) == 0 || callbacks[i].name[0] == '\0') {
      os_strncpy(callbacks[i].name, checkname, sizeof(checkname));
      callbacks[i].callback = cb;
      os_printf("CMD_AddCb: cb %s added at index %d\n", callbacks[i].name, i);
      break;
    }
  }
}

cmdCallback* ICACHE_FLASH_ATTR
CMD_GetCbByName(char* name) {
  char checkname[16];
  os_strncpy(checkname, name, sizeof(checkname));
  for (uint8_t i = 0; i < sizeof(commands); i++) {
    os_printf("CMD_GetCbByName: index %d name=%s cb=%p\n", i, callbacks[i].name, (void *)callbacks[i].callback);
    // if callback doesn't exist or it's null
    if (os_strcmp(callbacks[i].name, checkname) == 0) {
      os_printf("CMD_GetCbByName: cb %s found at index %d\n", callbacks[i].name, i);
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
    os_printf("CMD_WifiCb: wifiStatus=%d\n", wifiStatus);
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
  os_printf("CMD_WifiConnect: setup argc=%ld\n", CMD_GetArgc(&req));
	if(cmd->argc != 2 || cmd->callback == 0)
		return 0;

  wifiStatusCb = CMD_WifiCb;    // register our callback with wifi subsystem
  CMD_AddCb("wifiCb", (uint32_t)cmd->callback); // save the MCU's callback
  lastWifiStatus = wifiIsDisconnected;
  CMD_WifiCb(wifiState);

  return 1;
}

// Command handler for Wifi connect command
static uint32_t ICACHE_FLASH_ATTR
CMD_AddSensor(CmdPacket *cmd) {
  CmdRequest req;
  CMD_Request(&req, cmd);
  os_printf("CMD_AddSensor: setup argc=%ld\n", CMD_GetArgc(&req));
  if (cmd->argc != 1 || cmd->callback == 0)
    return 0;

  uint8_t* name;
  uint16_t len;

  // get the sensor name
  len = CMD_ArgLen(&req);
  os_printf("CMD_AddSensor: name len=%d\n", len);
  if (len > 15) return 0; // max size of name is 15 characters
  name = (uint8_t*)os_zalloc(len + 1);
  if (CMD_PopArg(&req, name, len)) return 0;
  name[len] = 0;
  os_printf("CMD_AddSensor: name=%s\n", name);

  CMD_AddCb((char*)name, (uint32_t)cmd->callback); // save the sensor callback 
  return 1;
}
