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

  // handle MQTT server settings
  int mqtt_server = 0; // accumulator for changes/errors
  mqtt_server |= getStringArg(connData, "mqtt-host",
      flashConfig.mqtt_hostname, sizeof(flashConfig.mqtt_hostname));
  if (mqtt_server < 0) return HTTPD_CGI_DONE;
  mqtt_server |= getStringArg(connData, "mqtt-client-id",
      flashConfig.mqtt_client, sizeof(flashConfig.mqtt_client));
  if (mqtt_server < 0) return HTTPD_CGI_DONE;
  mqtt_server |= getStringArg(connData, "mqtt-username",
      flashConfig.mqtt_username, sizeof(flashConfig.mqtt_username));
  if (mqtt_server < 0) return HTTPD_CGI_DONE;
  mqtt_server |= getStringArg(connData, "mqtt-password",
      flashConfig.mqtt_password, sizeof(flashConfig.mqtt_password));
  if (mqtt_server < 0) return HTTPD_CGI_DONE;
  mqtt_server |= getBoolArg(connData, "mqtt-enable",
      &flashConfig.mqtt_enable);

  // handle mqtt port
  char buff[16];
  if (httpdFindArg(connData->getArgs, "mqtt-port", buff, sizeof(buff)) > 0) {
    int32_t port = atoi(buff);
    if (port > 0 && port < 65536) {
      flashConfig.mqtt_port = port;
      mqtt_server |= 1;
    } else {
      errorResponse(connData, 400, "Invalid MQTT port");
      return HTTPD_CGI_DONE;
    }
  }

  // if server setting changed, we need to "make it so"
  if (mqtt_server) {
    os_printf("MQTT server settings changed, enable=%d\n", flashConfig.mqtt_enable);
    // TODO
  }

  // no action required if mqtt status settings change, they just get picked up at the
  // next status tick
  if (getBoolArg(connData, "mqtt-status-enable", &flashConfig.mqtt_status_enable) < 0)
    return HTTPD_CGI_DONE;
  if (getStringArg(connData, "mqtt-status-topic",
        flashConfig.mqtt_status_topic, sizeof(flashConfig.mqtt_status_topic)) < 0)
    return HTTPD_CGI_DONE;

  // if SLIP-enable is toggled it gets picked-up immediately by the parser
  int slip_update = getBoolArg(connData, "slip-enable", &flashConfig.slip_enable);
  if (slip_update < 0) return HTTPD_CGI_DONE;
  if (slip_update > 0) os_printf("SLIP-enable changed: %d\n", flashConfig.slip_enable);

  os_printf("Saving config\n");
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
