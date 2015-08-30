#include <esp8266.h>
//#include <mqtt.h>
//#include <cgiwifi.h>

//MQTT_Client mqttClient;
//
//void ICACHE_FLASH_ATTR 
//mqttConnectedCb(uint32_t *args) {
//  MQTT_Client* client = (MQTT_Client*)args;
//  MQTT_Publish(client, "announce/all", "Hello World!", 0, 0);
//}
//
//void ICACHE_FLASH_ATTR 
//mqttDisconnectedCb(uint32_t *args) {
////  MQTT_Client* client = (MQTT_Client*)args;
//  os_printf("MQTT Disconnected\n");
//}
//
//void ICACHE_FLASH_ATTR 
//mqttTcpDisconnectedCb(uint32_t *args) {
////  MQTT_Client* client = (MQTT_Client*)args;
//  os_printf("MQTT TCP Disconnected\n");
//}
//
//void ICACHE_FLASH_ATTR 
//mqttPublishedCb(uint32_t *args) {
////  MQTT_Client* client = (MQTT_Client*)args;
//  os_printf("MQTT Published\n");
//}
//
//void ICACHE_FLASH_ATTR 
//mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len) {
//  char *topicBuf = (char*)os_zalloc(topic_len + 1);
//  char *dataBuf = (char*)os_zalloc(data_len + 1);
//
////  MQTT_Client* client = (MQTT_Client*)args;
//
//  os_memcpy(topicBuf, topic, topic_len);
//  topicBuf[topic_len] = 0;
//
//  os_memcpy(dataBuf, data, data_len);
//  dataBuf[data_len] = 0;
//
//  os_printf("Receive topic: %s, data: %s\n", topicBuf, dataBuf);
//  os_free(topicBuf);
//  os_free(dataBuf);
//}
//
//void ICACHE_FLASH_ATTR 
//wifiStateChangeCb(uint8_t status)
//{
//  if (status == wifiGotIP  && mqttClient.connState != TCP_CONNECTING){
//    MQTT_Connect(&mqttClient);
//  }
//  else if (status == wifiIsDisconnected && mqttClient.connState == TCP_CONNECTING){    
//    MQTT_Disconnect(&mqttClient);
//  }
//}

void init() {
//  wifiAddStateChangeCb(wifiStateChangeCb);
//  MQTT_InitConnection(&mqttClient, MQTT_HOST, MQTT_PORT, MQTT_SECURITY);
//  MQTT_InitClient(&mqttClient, MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS, MQTT_KEEPALIVE, MQTT_CLSESSION);
//  MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
//  MQTT_OnConnected(&mqttClient, mqttConnectedCb);
//  MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
//  MQTT_OnDisconnected(&mqttClient, mqttTcpDisconnectedCb);
//  MQTT_OnPublished(&mqttClient, mqttPublishedCb);
//  MQTT_OnData(&mqttClient, mqttDataCb);
}