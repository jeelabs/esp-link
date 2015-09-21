#ifdef MQTT
#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H
#include "mqtt.h"

extern MQTT_Client mqttClient;
extern char* statusTopicStr;
void mqtt_client_init();
void mqtt_client_on_connected(MqttCallback connectedCb);
void mqtt_client_on_disconnected(MqttCallback disconnectedCb);
void mqtt_client_on_published(MqttCallback publishedCb);
void mqtt_client_on_data(MqttDataCallback dataCb);

#endif //MQTT_CLIENT_H
#endif // MQTT
