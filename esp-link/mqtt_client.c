#include <esp8266.h>
#include "cgiwifi.h"
#include "config.h"
#include "mqtt.h"

MQTT_Client mqttClient;

static ETSTimer mqttTimer;

static int once = 0;
static void ICACHE_FLASH_ATTR mqttTimerCb(void *arg) {
  if (once++ > 0) return;
  MQTT_Init(&mqttClient, flashConfig.mqtt_hostname, flashConfig.mqtt_port, 0, 2,
      flashConfig.mqtt_client, flashConfig.mqtt_username, flashConfig.mqtt_password, 60);
  MQTT_Connect(&mqttClient);
  MQTT_Subscribe(&mqttClient, "system/time", 0);
}

void ICACHE_FLASH_ATTR
wifiStateChangeCb(uint8_t status)
{
  if (status == wifiGotIP) {
    os_timer_disarm(&mqttTimer);
    os_timer_setfn(&mqttTimer, mqttTimerCb, NULL);
    os_timer_arm(&mqttTimer, 200, 0);
  }
}


// initialize the custom stuff that goes beyond esp-link
void mqtt_client_init() {
  wifiAddStateChangeCb(wifiStateChangeCb);
}


#if 0
MQTT_Client mqttClient;

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
  char *topicBuf = (char*)os_zalloc(topic_len + 1);
  char *dataBuf = (char*)os_zalloc(data_len + 1);

//  MQTT_Client* client = (MQTT_Client*)args;

  os_memcpy(topicBuf, topic, topic_len);
  topicBuf[topic_len] = 0;

  os_memcpy(dataBuf, data, data_len);
  dataBuf[data_len] = 0;

  os_printf("Receive topic: %s, data: %s\n", topicBuf, dataBuf);
  os_free(topicBuf);
  os_free(dataBuf);
}

  MQTT_InitConnection(&mqttClient, MQTT_HOST, MQTT_PORT, MQTT_SECURITY);
  MQTT_InitClient(&mqttClient, MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS, MQTT_KEEPALIVE, MQTT_CLSESSION);
  MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
  MQTT_OnConnected(&mqttClient, mqttConnectedCb);
  MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
  MQTT_OnDisconnected(&mqttClient, mqttTcpDisconnectedCb);
  MQTT_OnPublished(&mqttClient, mqttPublishedCb);
  MQTT_OnData(&mqttClient, mqttDataCb);
#endif
