//
// MQTT Commands coming in from the attache microcontrollver over the serial port
//

#include <esp8266.h>
#include "mqtt.h"
#include "mqtt_client.h"
#include "mqtt_cmd.h"

#ifdef MQTTCMD_DBG
#define DBG(format, ...) os_printf(format, ## __VA_ARGS__)
#else
#define DBG(format, ...) do { } while(0)
#endif

void ICACHE_FLASH_ATTR
cmdMqttConnectedCb(MQTT_Client* client) {
  MqttCmdCb* cb = (MqttCmdCb*)client->user_data;
  DBG("MQTT: Connected Cb=%p\n", (void*)cb->connectedCb);
  cmdResponseStart(CMD_RESP_CB, cb->connectedCb, 0);
  cmdResponseEnd();
}

void ICACHE_FLASH_ATTR
cmdMqttDisconnectedCb(MQTT_Client* client) {
  MqttCmdCb* cb = (MqttCmdCb*)client->user_data;
  DBG("MQTT: Disconnected cb=%p\n", (void*)cb->disconnectedCb);
  cmdResponseStart(CMD_RESP_CB, cb->disconnectedCb, 0);
  cmdResponseEnd();
}

void ICACHE_FLASH_ATTR
cmdMqttPublishedCb(MQTT_Client* client) {
  MqttCmdCb* cb = (MqttCmdCb*)client->user_data;
  DBG("MQTT: Published cb=%p\n", (void*)cb->publishedCb);
  cmdResponseStart(CMD_RESP_CB, cb->publishedCb, 0);
  cmdResponseEnd();
}

void ICACHE_FLASH_ATTR
cmdMqttDataCb(MQTT_Client* client, const char* topic, uint32_t topic_len,
    const char* data, uint32_t data_len)
{
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
  cmdPopArg(&req, (uint8_t*)&client->connect_info.will_qos, 4);

  // get retain
  cmdPopArg(&req, (uint8_t*)&client->connect_info.will_retain, 4);

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
  cmdPopArg(&req, (uint8_t*)&qos, 4);

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

#if 0
  if (cmdGetArgc(&req) != 9)
    return 0;

  // create mqtt client
  uint8_t clientLen = sizeof(MQTT_Client);
  MQTT_Client* client = (MQTT_Client*)os_zalloc(clientLen);
  if (client == NULL) return 0;
  os_memset(client, 0, clientLen);

  uint16_t len;
  uint8_t *client_id, *user_data, *pass_data;
  uint32_t keepalive, clean_session;

  // get client id
  len = cmdArgLen(&req);
  if (len > 32) return 0; // safety check
  client_id = (uint8_t*)os_zalloc(len + 1);
  cmdPopArg(&req, client_id, len);
  client_id[len] = 0;

  // get username
  len = cmdArgLen(&req);
  if (len > 32) return 0; // safety check
  user_data = (uint8_t*)os_zalloc(len + 1);
  cmdPopArg(&req, user_data, len);
  user_data[len] = 0;

  // get password
  len = cmdArgLen(&req);
  if (len > 32) return 0; // safety check
  pass_data = (uint8_t*)os_zalloc(len + 1);
  cmdPopArg(&req, pass_data, len);
  pass_data[len] = 0;

  // get keepalive
  cmdPopArg(&req, (uint8_t*)&keepalive, 4);

  // get clean session
  cmdPopArg(&req, (uint8_t*)&clean_session, 4);
#ifdef MQTTCMD_DBG
  DBG("MQTT: MQTTCMD_Setup clientid=%s, user=%s, pw=%s, keepalive=%ld, clean_session=%ld\n", client_id, user_data, pass_data, keepalive, clean_session);
#endif

  // init client
  // TODO: why malloc these all here, pass to MQTT_InitClient to be malloc'd again?
  MQTT_InitClient(client, (char*)client_id, (char*)user_data, (char*)pass_data, keepalive, clean_session);

  os_free(client_id);
  os_free(user_data);
  os_free(pass_data);
#endif

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

#if 0
uint32_t ICACHE_FLASH_ATTR
MQTTCMD_Connect(CmdPacket *cmd) {
  CmdRequest req;
  cmdRequest(&req, cmd);

#ifdef MQTT_1_CLIENT

  if (mqttClient.connState == MQTT_CONNECTED && mqttClient.cmdConnectedCb) {
    mqttClient.cmdConnectedCb((uint32_t*)&mqttClient);
  }
  else if (mqttClient.connState == MQTT_DISCONNECTED && mqttClient.cmdDisconnectedCb) {
    mqttClient.cmdDisconnectedCb((uint32_t*)&mqttClient);
  }

  return 1;

#else
  if (cmdGetArgc(&req) != 4)
    return 0;

  // get mqtt client
  uint32_t client_ptr;
  cmdPopArg(&req, (uint8_t*)&client_ptr, 4);
  MQTT_Client* client = (MQTT_Client*)client_ptr;
  DBG("MQTT: MQTTCMD_Connect client ptr=%p\n", (void*)client_ptr);

  uint16_t len;

  // get host
  if (client->host)
  os_free(client->host);
  len = cmdArgLen(&req);
  if (len > 128) return 0; // safety check
  client->host = (char*)os_zalloc(len + 1);
  cmdPopArg(&req, client->host, len);
  client->host[len] = 0;

  // get port
  cmdPopArg(&req, (uint8_t*)&client->port, 4);

  // get security
  cmdPopArg(&req, (uint8_t*)&client->security, 4);
  DBG("MQTT: MQTTCMD_Connect host=%s, port=%d, security=%d\n",
    client->host,
    client->port,
    client->security);

  MQTT_Connect(client);
  return 1;
#endif
}

uint32_t ICACHE_FLASH_ATTR
MQTTCMD_Disconnect(CmdPacket *cmd) {
  CmdRequest req;
  cmdRequest(&req, cmd);

#ifdef MQTT_1_CLIENT
  return 1;

#else
  if (cmdGetArgc(&req) != 1)
    return 0;

  // get mqtt client
  uint32_t client_ptr;
  cmdPopArg(&req, (uint8_t*)&client_ptr, 4);
  MQTT_Client* client = (MQTT_Client*)client_ptr;
  DBG("MQTT: MQTTCMD_Disconnect client ptr=%p\n", (void*)client_ptr);

  // disconnect
  MQTT_Disconnect(client);
  return 1;
#endif
}
#endif
