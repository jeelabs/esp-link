// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt
// // TCP Client settings

#include <esp8266.h>
#include "cgi.h"
#include "config.h"
#include "cgimqtt.h"

// Cgi to return MQTT settings
int ICACHE_FLASH_ATTR cgiMqttGet(HttpdConnData *connData) {
  char buff[2048];
  int len;

  if (connData->conn==NULL) return HTTPD_CGI_DONE;

  len = os_sprintf(buff, "{ "
      "\"slip-enable\":%d, "
      "\"mqtt-enable\":%d, "
      "\"mqtt-status-enable\":%d, "
      "\"mqtt-port\":%d, "
      "\"mqtt-host\":\"%s\", "
      "\"mqtt-client-id\":\"%s\", "
      "\"mqtt-username\":\"%s\", "
      "\"mqtt-password\":\"%s\", "
      "\"mqtt-status-topic\":\"%s\", "
      "\"mqtt-state\":\"%s\" }",
      flashConfig.slip_enable, flashConfig.mqtt_enable, flashConfig.mqtt_status_enable,
      flashConfig.mqtt_port, flashConfig.mqtt_hostname, flashConfig.mqtt_client,
      flashConfig.mqtt_username, flashConfig.mqtt_password,
      flashConfig.mqtt_status_topic, "connected");

  jsonHeader(connData, 200);
  httpdSend(connData, buff, len);
  return HTTPD_CGI_DONE;
}

// Cgi to change choice of pin assignments
int ICACHE_FLASH_ATTR cgiMqttSet(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE;

#if 0
  // Handle tcp_enable flag
  char buff[128];
  int len = httpdFindArg(connData->getArgs, "tcp_enable", buff, sizeof(buff));
  if (len <= 0) {
    jsonHeader(connData, 400);
    return HTTPD_CGI_DONE;
  }
  flashConfig.tcp_enable = os_strcmp(buff, "true") == 0;

  // Handle rssi_enable flag
  len = httpdFindArg(connData->getArgs, "rssi_enable", buff, sizeof(buff));
  if (len <= 0) {
    jsonHeader(connData, 400);
    return HTTPD_CGI_DONE;
  }
  flashConfig.rssi_enable = os_strcmp(buff, "true") == 0;

  // Handle api_key flag
  len = httpdFindArg(connData->getArgs, "api_key", buff, sizeof(buff));
  if (len < 0) {
    jsonHeader(connData, 400);
    return HTTPD_CGI_DONE;
  }
  buff[sizeof(flashConfig.api_key)-1] = 0; // ensure we don't get an overrun
  os_strcpy(flashConfig.api_key, buff);
#endif

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

int ICACHE_FLASH_ATTR cgiMqtt(HttpdConnData *connData) {
  if (connData->requestType == HTTPD_METHOD_GET) {
    return cgiMqttGet(connData);
  } else if (connData->requestType == HTTPD_METHOD_POST) {
    return cgiMqttSet(connData);
  } else {
    jsonHeader(connData, 404);
    return HTTPD_CGI_DONE;
  }
}
