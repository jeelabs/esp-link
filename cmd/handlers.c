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

static uint32_t ICACHE_FLASH_ATTR CMD_Null(CmdPacket *cmd);
static uint32_t ICACHE_FLASH_ATTR CMD_IsReady(CmdPacket *cmd);
static uint32_t ICACHE_FLASH_ATTR CMD_WifiConnect(CmdPacket *cmd);

// Command dispatch table for serial -> ESP commands
const CmdList commands[] = {
  {CMD_NULL,            CMD_Null},
  {CMD_RESET,           CMD_Null},
  {CMD_IS_READY,        CMD_IsReady},
  {CMD_WIFI_CONNECT,    CMD_WifiConnect},

/*
  {CMD_MQTT_SETUP,      MQTTAPP_Setup},
  {CMD_MQTT_CONNECT,    MQTTAPP_Connect},
  {CMD_MQTT_DISCONNECT, MQTTAPP_Disconnect},
  {CMD_MQTT_PUBLISH,    MQTTAPP_Publish},
  {CMD_MQTT_SUBSCRIBE , MQTTAPP_Subscribe},
  {CMD_MQTT_LWT,        MQTTAPP_Lwt},
  */

  {CMD_REST_SETUP,      REST_Setup},
  {CMD_REST_REQUEST,    REST_Request},
  {CMD_REST_SETHEADER,  REST_SetHeader},
  {CMD_NULL,            NULL}
};

// Command handler for IsReady (healthcheck) command
static uint32_t ICACHE_FLASH_ATTR
CMD_IsReady(CmdPacket *cmd) {
  os_printf("CMD: Check ready\n");
  return 1;
}

// Command handler for Null command
static uint32_t ICACHE_FLASH_ATTR
CMD_Null(CmdPacket *cmd) {
  os_printf("CMD: NULL/unsupported command\n");
  return 1;
}

static uint8_t lastWifiStatus;
static uint32_t wifiCallback;

// Callback from wifi subsystem to notify us of status changes
static void ICACHE_FLASH_ATTR
CMD_WifiCb(uint8_t wifiStatus) {
  if (wifiStatus != lastWifiStatus){
    lastWifiStatus = wifiStatus;
    if (wifiCallback) {
      uint8_t status = wifiStatus == wifiGotIP ? 5 : 1;
      uint16_t crc = CMD_ResponseStart(CMD_WIFI_CONNECT, wifiCallback, 0, 1);
      crc = CMD_ResponseBody(crc, (uint8_t*)&status, 1);
      CMD_ResponseEnd(crc);
    }
  }
}

// Command handler for Wifi connect command
static uint32_t ICACHE_FLASH_ATTR
CMD_WifiConnect(CmdPacket *cmd) {
	if(cmd->argc != 2 || cmd->callback == 0)
		return 0xFFFFFFFF;

  wifiStatusCb = CMD_WifiCb;    // register our callback with wifi subsystem
	wifiCallback = cmd->callback; // save the MCU's callback
  return 1;
}
