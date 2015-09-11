#include <esp8266.h>
#include "cgiwifi.h"
#include <mqtt.h>
#include <config.h>
#include "latch_json.h"
#include "user_funcs.h"



MQTT_Client mqttClient;
//static ETSTimer mqttTimer;

void ICACHE_FLASH_ATTR
mqttConnectedCb(uint32_t *args) {
  MQTT_Client* client = (MQTT_Client*)args;
  MQTT_Publish(client, "announce/all", "Hello World!", 0, 0);
}

void ICACHE_FLASH_ATTR
mqttDisconnectedCb(uint32_t *args) {
//  MQTT_Client* client = (MQTT_Client*)args;
  os_printf("MQTT Disconnected\n");
}

void ICACHE_FLASH_ATTR
mqttTcpDisconnectedCb(uint32_t *args) {
//  MQTT_Client* client = (MQTT_Client*)args;
  os_printf("MQTT TCP Disconnected\n");
}

void ICACHE_FLASH_ATTR
mqttPublishedCb(uint32_t *args) {
//  MQTT_Client* client = (MQTT_Client*)args;
  os_printf("MQTT Published\n");
}

void ICACHE_FLASH_ATTR
mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len) {
//  MQTT_Client* client = (MQTT_Client*)args;
  char *topicBuf = (char*)os_zalloc(topic_len + 1);
  char *dataBuf = (char*)os_zalloc(data_len + 1);

  os_memcpy(topicBuf, topic, topic_len);
  topicBuf[topic_len] = 0;

  os_memcpy(dataBuf, data, data_len);
  dataBuf[data_len] = 0;

  os_printf("Receive topic: %s, data: %s\n", topicBuf, dataBuf);
  os_free(topicBuf);
  os_free(dataBuf);
}

void ICACHE_FLASH_ATTR
wifiStateChangeCb(uint8_t status) {
  if (flashConfig.mqtt_enable) {
    if (status == wifiGotIP  && mqttClient.connState != TCP_CONNECTING) {   
      MQTT_Connect(&mqttClient);
    }
    else if (status == wifiIsDisconnected && mqttClient.connState == TCP_CONNECTING) {
      MQTT_Disconnect(&mqttClient);
    }
  }
}

// initialize the custom stuff that goes beyond esp-link
void app_init() {
  if (flashConfig.mqtt_enable) {
    MQTT_Init(&mqttClient, flashConfig.mqtt_host, flashConfig.mqtt_port, 0, flashConfig.mqtt_timeout,
      flashConfig.mqtt_clientid, flashConfig.mqtt_username, flashConfig.mqtt_password, flashConfig.mqtt_keepalive, flashConfig.mqtt_clean_session
    );

    MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
    MQTT_OnConnected(&mqttClient, mqttConnectedCb);
    MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
    MQTT_OnDisconnected(&mqttClient, mqttTcpDisconnectedCb);
    MQTT_OnPublished(&mqttClient, mqttPublishedCb);
    MQTT_OnData(&mqttClient, mqttDataCb);
  }

  wifiAddStateChangeCb(wifiStateChangeCb);
}

