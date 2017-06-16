#ifdef MQTT
#include <esp8266.h>
#include "cgiwifi.h"
#include "config.h"
#include "mqtt.h"


#ifdef MQTTCLIENT_DBG
#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)
#else
#define DBG(format, ...) do { } while(0)
#endif

MQTT_Client mqttClient; // main mqtt client used by esp-link

static MqttCallback connected_cb;
static MqttCallback disconnected_cb;
static MqttCallback published_cb;
static MqttDataCallback data_cb;

void ICACHE_FLASH_ATTR
mqttConnectedCb(MQTT_Client* client) {
  DBG("MQTT Client: Connected\n");
  //MQTT_Subscribe(client, "system/time", 0); // handy for testing
  if (connected_cb)
    connected_cb(client);
}

void ICACHE_FLASH_ATTR
mqttDisconnectedCb(MQTT_Client* client) {
  DBG("MQTT Client: Disconnected\n");
  if (disconnected_cb)
    disconnected_cb(client);
}

void ICACHE_FLASH_ATTR
mqttPublishedCb(MQTT_Client* client) {
  DBG("MQTT Client: Published\n");
  if (published_cb)
    published_cb(client);
}

void ICACHE_FLASH_ATTR
mqttDataCb(MQTT_Client* client, const char* topic, uint32_t topic_len,
    const char *data, uint32_t data_len)
{
#ifdef MQTTCLIENT_DBG
  char *topicBuf = (char*)os_zalloc(topic_len + 1);
  char *dataBuf = (char*)os_zalloc(data_len + 1);

  os_memcpy(topicBuf, topic, topic_len);
  topicBuf[topic_len] = 0;

  os_memcpy(dataBuf, data, data_len);
  dataBuf[data_len] = 0;

  os_printf("MQTT Client: Received topic: %s, data: %s\n", topicBuf, dataBuf);
  os_free(topicBuf);
  os_free(dataBuf);
#endif

  if (data_cb)
    data_cb(client, topic, topic_len, data, data_len);
}

void ICACHE_FLASH_ATTR
wifiStateChangeCb(uint8_t status)
{
  if (flashConfig.mqtt_enable) {
    if (status == wifiGotIP && mqttClient.connState < TCP_CONNECTING) {
      MQTT_Connect(&mqttClient);
    }
    else if (status == wifiIsDisconnected && mqttClient.connState == TCP_CONNECTING) {
      MQTT_Disconnect(&mqttClient);
    }
  }
}

void ICACHE_FLASH_ATTR
mqtt_client_init()
{
  MQTT_Init(&mqttClient, flashConfig.mqtt_host, flashConfig.mqtt_port, 0, flashConfig.mqtt_timeout,
    flashConfig.mqtt_clientid, flashConfig.mqtt_username, flashConfig.mqtt_password,
    flashConfig.mqtt_keepalive);

  MQTT_OnConnected(&mqttClient, mqttConnectedCb);
  MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
  MQTT_OnPublished(&mqttClient, mqttPublishedCb);
  MQTT_OnData(&mqttClient, mqttDataCb);

  // Don't connect now, wait for a wifi status change callback
  //if (flashConfig.mqtt_enable && strlen(flashConfig.mqtt_host) > 0)
  //  MQTT_Connect(&mqttClient);

  wifiAddStateChangeCb(wifiStateChangeCb);
}

void ICACHE_FLASH_ATTR
mqtt_client_on_connected(MqttCallback connectedCb) {
  connected_cb = connectedCb;
}

void ICACHE_FLASH_ATTR
mqtt_client_on_disconnected(MqttCallback disconnectedCb) {
  disconnected_cb = disconnectedCb;
}

void ICACHE_FLASH_ATTR
mqtt_client_on_published(MqttCallback publishedCb) {
  published_cb = publishedCb;
}

void ICACHE_FLASH_ATTR
mqtt_client_on_data(MqttDataCallback dataCb) {
  data_cb = dataCb;
}
#endif // MQTT
