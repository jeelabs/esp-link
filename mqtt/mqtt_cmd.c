//
// MQTT Commands coming in from the attache microcontrollver over the serial port
//

#include <esp8266.h>
#include "mqtt.h"
#include "mqtt_client.h"
#include "mqtt_cmd.h"

#ifdef MQTTCMD_DBG
#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)
#else
#define DBG(format, ...) do { } while(0)
#endif

static bool blocked; // flag to prevent MQTT from sending on serial while trying to PGM uC

void ICACHE_FLASH_ATTR
mqtt_block() { blocked = true; }
void ICACHE_FLASH_ATTR
mqtt_unblock() { blocked = false; }

void ICACHE_FLASH_ATTR
cmdMqttConnectedCb(MQTT_Client* client) {
  if (blocked) return;
  MqttCmdCb* cb = (MqttCmdCb*)client->user_data;
  DBG("MQTT: Connected Cb=%p\n", (void*)cb->connectedCb);
  cmdResponseStart(CMD_RESP_CB, cb->connectedCb, 0);
  cmdResponseEnd();
}

void ICACHE_FLASH_ATTR
cmdMqttDisconnectedCb(MQTT_Client* client) {
  if (blocked) return;
  MqttCmdCb* cb = (MqttCmdCb*)client->user_data;
  DBG("MQTT: Disconnected cb=%p\n", (void*)cb->disconnectedCb);
  cmdResponseStart(CMD_RESP_CB, cb->disconnectedCb, 0);
  cmdResponseEnd();
}

void ICACHE_FLASH_ATTR
cmdMqttPublishedCb(MQTT_Client* client) {
  if (blocked) return;
  MqttCmdCb* cb = (MqttCmdCb*)client->user_data;
  DBG("MQTT: Published cb=%p\n", (void*)cb->publishedCb);
  cmdResponseStart(CMD_RESP_CB, cb->publishedCb, 0);
  cmdResponseEnd();
}

void ICACHE_FLASH_ATTR
cmdMqttDataCb(MQTT_Client* client, const char* topic, uint32_t topic_len,
    const char* data, uint32_t data_len)
{
  if (blocked) return;
  MqttCmdCb* cb = (MqttCmdCb*)client->user_data;
  DBG("MQTT: Data cb=%p topic=%s len=%u\n", (void*)cb->dataCb, topic, data_len);

  cmdResponseStart(CMD_RESP_CB, cb->dataCb, 2);
  cmdResponseBody(topic, topic_len);
  cmdResponseBody(data, data_len);
  cmdResponseEnd();
}

void ICACHE_FLASH_ATTR
MQTTCMD_Lwt(CmdPacket *cmd) {
  CmdRequest req;
  cmdRequest(&req, cmd);

  if (cmdGetArgc(&req) != 4) return;

  MQTT_Client* client = &mqttClient;

  // free old topic & message
  if (client->connect_info.will_topic)
    os_free(client->connect_info.will_topic);
  if (client->connect_info.will_message)
    os_free(client->connect_info.will_message);

  uint16_t len;

  // get topic
  len = cmdArgLen(&req);
  if (len > 128) return; // safety check
  client->connect_info.will_topic = (char*)os_zalloc(len + 1);
  cmdPopArg(&req, client->connect_info.will_topic, len);
  client->connect_info.will_topic[len] = 0;

  // get message
  len = cmdArgLen(&req);
  if (len > 128) return; // safety check
  client->connect_info.will_message = (char*)os_zalloc(len + 1);
  cmdPopArg(&req, client->connect_info.will_message, len);
  client->connect_info.will_message[len] = 0;

  // get qos
  cmdPopArg(&req, (uint8_t*)&client->connect_info.will_qos, sizeof(client->connect_info.will_qos));

  // get retain
  cmdPopArg(&req, (uint8_t*)&client->connect_info.will_retain, sizeof(client->connect_info.will_retain));

  DBG("MQTT: MQTTCMD_Lwt topic=%s, message=%s, qos=%d, retain=%d\n",
       client->connect_info.will_topic,
       client->connect_info.will_message,
       client->connect_info.will_qos,
       client->connect_info.will_retain);

  // trigger a reconnect to set the LWT
  MQTT_Reconnect(client);
}

void ICACHE_FLASH_ATTR
MQTTCMD_Publish(CmdPacket *cmd) {
  CmdRequest req;
  cmdRequest(&req, cmd);

  if (cmdGetArgc(&req) != 5) return;

  MQTT_Client* client = &mqttClient;

  uint16_t len;

  // get topic
  len = cmdArgLen(&req);
  if (len > 128) return; // safety check
  uint8_t *topic = (uint8_t*)os_zalloc(len + 1);
  cmdPopArg(&req, topic, len);
  topic[len] = 0;

  // get data
  len = cmdArgLen(&req);
  uint8_t *data = (uint8_t*)os_zalloc(len+1);
  if (!data) { // safety check
    os_free(topic);
    return;
  }
  cmdPopArg(&req, data, len);
  data[len] = 0;

  uint16_t data_len;
  uint8_t qos, retain;

  // get data length
  cmdPopArg(&req, &data_len, sizeof(data_len));

  // get qos
  cmdPopArg(&req, &qos, sizeof(qos));

  // get retain
  cmdPopArg(&req, &retain, sizeof(retain));

  DBG("MQTT: MQTTCMD_Publish topic=%s, data_len=%d, qos=%d, retain=%d\n",
    topic, data_len, qos, retain);

  MQTT_Publish(client, (char*)topic, (char*)data, data_len, qos%3, retain&1);
  os_free(topic);
  os_free(data);
  return;
}

void ICACHE_FLASH_ATTR
MQTTCMD_Subscribe(CmdPacket *cmd) {
  CmdRequest req;
  cmdRequest(&req, cmd);

  if (cmdGetArgc(&req) != 2) return;

  MQTT_Client* client = &mqttClient;

  uint16_t len;

  // get topic
  len = cmdArgLen(&req);
  if (len > 128) return; // safety check
  uint8_t* topic = (uint8_t*)os_zalloc(len + 1);
  cmdPopArg(&req, topic, len);
  topic[len] = 0;

  // get qos
  uint32_t qos = 0;
  cmdPopArg(&req, (uint8_t*)&qos, sizeof(qos));

  DBG("MQTT: MQTTCMD_Subscribe topic=%s, qos=%u\n", topic, qos);

  MQTT_Subscribe(client, (char*)topic, (uint8_t)qos);
  os_free(topic);
  return;
}

void ICACHE_FLASH_ATTR
MQTTCMD_Setup(CmdPacket *cmd) {
  CmdRequest req;
  cmdRequest(&req, cmd);

  MQTT_Client* client = &mqttClient;

  if (cmdGetArgc(&req) != 4) return;

  // create callback
  MqttCmdCb* callback = (MqttCmdCb*)os_zalloc(sizeof(MqttCmdCb));
  cmdPopArg(&req, &callback->connectedCb, 4);
  cmdPopArg(&req, &callback->disconnectedCb, 4);
  cmdPopArg(&req, &callback->publishedCb, 4);
  cmdPopArg(&req, &callback->dataCb, 4);
  client->user_data = callback;

  DBG("MQTT connectedCb=%x\n", callback->connectedCb);

  client->cmdConnectedCb = cmdMqttConnectedCb;
  client->cmdDisconnectedCb = cmdMqttDisconnectedCb;
  client->cmdPublishedCb = cmdMqttPublishedCb;
  client->cmdDataCb = cmdMqttDataCb;

  if (client->connState == MQTT_CONNECTED) {
    if (callback->connectedCb)
      cmdMqttConnectedCb(client);
  } else if (callback->disconnectedCb) {
    cmdMqttDisconnectedCb(client);
  }
}
