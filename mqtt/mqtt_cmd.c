#include <esp8266.h>
#include "mqtt.h"
#include "mqtt_cmd.h"

uint32_t connectedCb = 0, disconnectCb = 0, tcpDisconnectedCb = 0, publishedCb = 0, dataCb = 0;

void ICACHE_FLASH_ATTR
cmdMqttConnectedCb(uint32_t* args) {
  MQTT_Client* client = (MQTT_Client*)args;
  MqttCmdCb* cb = (MqttCmdCb*)client->user_data;
#ifdef MQTTCMD_DBG
  os_printf("MQTT: Connected  connectedCb=%p, disconnectedCb=%p, publishedCb=%p, dataCb=%p\n",
       (void*)cb->connectedCb,
       (void*)cb->disconnectedCb,
       (void*)cb->publishedCb,
       (void*)cb->dataCb);
#endif
  uint16_t crc = CMD_ResponseStart(CMD_MQTT_EVENTS, cb->connectedCb, 0, 0);
  CMD_ResponseEnd(crc);
}

void ICACHE_FLASH_ATTR
cmdMqttTcpDisconnectedCb(uint32_t *args) {
  MQTT_Client* client = (MQTT_Client*)args;
  MqttCmdCb *cb = (MqttCmdCb*)client->user_data;
#ifdef MQTTCMD_DBG
  os_printf("MQTT: TCP Disconnected\n");
#endif
  uint16_t crc = CMD_ResponseStart(CMD_MQTT_EVENTS, cb->tcpDisconnectedCb, 0, 0);
  CMD_ResponseEnd(crc);
}

void ICACHE_FLASH_ATTR
cmdMqttDisconnectedCb(uint32_t* args) {
  MQTT_Client* client = (MQTT_Client*)args;
  MqttCmdCb* cb = (MqttCmdCb*)client->user_data;
#ifdef MQTTCMD_DBG
  os_printf("MQTT: Disconnected\n");
#endif
  uint16_t crc = CMD_ResponseStart(CMD_MQTT_EVENTS, cb->disconnectedCb, 0, 0);
  CMD_ResponseEnd(crc);
}

void ICACHE_FLASH_ATTR
cmdMqttPublishedCb(uint32_t* args) {
  MQTT_Client* client = (MQTT_Client*)args;
  MqttCmdCb* cb = (MqttCmdCb*)client->user_data;
#ifdef MQTTCMD_DBG
  os_printf("MQTT: Published\n");
#endif
  uint16_t crc = CMD_ResponseStart(CMD_MQTT_EVENTS, cb->publishedCb, 0, 0);
  CMD_ResponseEnd(crc);
}

void ICACHE_FLASH_ATTR
cmdMqttDataCb(uint32_t* args, const char* topic, uint32_t topic_len, const char* data, uint32_t data_len) {
  uint16_t crc = 0;
  MQTT_Client* client = (MQTT_Client*)args;
  MqttCmdCb* cb = (MqttCmdCb*)client->user_data;

  crc = CMD_ResponseStart(CMD_MQTT_EVENTS, cb->dataCb, 0, 2);
  crc = CMD_ResponseBody(crc, (uint8_t*)topic, topic_len);
  crc = CMD_ResponseBody(crc, (uint8_t*)data, data_len);
  CMD_ResponseEnd(crc);
}

uint32_t ICACHE_FLASH_ATTR
MQTTCMD_Setup(CmdPacket *cmd) {
  CmdRequest req;
  CMD_Request(&req, cmd);

  if (CMD_GetArgc(&req) != 9)
    return 0;

  // create mqtt client
  uint8_t clientLen = sizeof(MQTT_Client);
  MQTT_Client* client = (MQTT_Client*)os_zalloc(clientLen);
  if (client == NULL) return 0;
  os_memset(client, 0, clientLen);

  return 0;
#if 0

  uint16_t len;
  uint8_t *client_id, *user_data, *pass_data;
  uint32_t keepalive, clean_session, cb_data;

  // get client id
  len = CMD_ArgLen(&req);
  if (len > 32) return 0; // safety check
  client_id = (uint8_t*)os_zalloc(len + 1);
  CMD_PopArg(&req, client_id, len);
  client_id[len] = 0;

  // get username
  len = CMD_ArgLen(&req);
  if (len > 32) return 0; // safety check
  user_data = (uint8_t*)os_zalloc(len + 1);
  CMD_PopArg(&req, user_data, len);
  user_data[len] = 0;

  // get password
  len = CMD_ArgLen(&req);
  if (len > 32) return 0; // safety check
  pass_data = (uint8_t*)os_zalloc(len + 1);
  CMD_PopArg(&req, pass_data, len);
  pass_data[len] = 0;

  // get keepalive
  CMD_PopArg(&req, (uint8_t*)&keepalive, 4);

  // get clean session
  CMD_PopArg(&req, (uint8_t*)&clean_session, 4);
#ifdef MQTTCMD_DBG
  os_printf("MQTT: MQTTCMD_Setup clientid=%s, user=%s, pw=%s, keepalive=%ld, clean_session=%ld\n", client_id, user_data, pass_data, keepalive, clean_session);
#endif

  // init client
  // TODO: why malloc these all here, pass to MQTT_InitClient to be malloc'd again?
  MQTT_InitClient(client, (char*)client_id, (char*)user_data, (char*)pass_data, keepalive, clean_session);

  // create callback
  MqttCmdCb* callback = (MqttCmdCb*)os_zalloc(sizeof(MqttCmdCb));

  CMD_PopArg(&req, (uint8_t*)&cb_data, 4);
  callback->connectedCb = cb_data;
  CMD_PopArg(&req, (uint8_t*)&cb_data, 4);
  callback->disconnectedCb = cb_data;
  CMD_PopArg(&req, (uint8_t*)&cb_data, 4);
  callback->publishedCb = cb_data;
  CMD_PopArg(&req, (uint8_t*)&cb_data, 4);
  callback->dataCb = cb_data;

  client->user_data = callback;

  client->cmdConnectedCb = cmdMqttConnectedCb;
  client->cmdDisconnectedCb = cmdMqttDisconnectedCb;
  client->cmdPublishedCb = cmdMqttPublishedCb;
  client->cmdDataCb = cmdMqttDataCb;

  if (CMD_GetArgc(&req) == 10) {
    CMD_PopArg(&req, (uint8_t*)&cb_data, 4);
    callback->tcpDisconnectedCb = cb_data;
    client->cmdTcpDisconnectedCb = cmdMqttTcpDisconnectedCb;
  }

  os_free(client_id);
  os_free(user_data);
  os_free(pass_data);

  return (uint32_t)client;
#endif
}

uint32_t ICACHE_FLASH_ATTR
MQTTCMD_Lwt(CmdPacket *cmd) {
  CmdRequest req;
  CMD_Request(&req, cmd);

  if (CMD_GetArgc(&req) != 5)
    return 0;

  // get mqtt client
  uint32_t client_ptr;
  CMD_PopArg(&req, (uint8_t*)&client_ptr, 4);
  MQTT_Client* client = (MQTT_Client*)client_ptr;
#ifdef MQTTCMD_DBG
  os_printf("MQTT: MQTTCMD_Lwt client ptr=%p\n", (void*)client_ptr);
#endif

  uint16_t len;

  // get topic
  if (client->connect_info.will_topic)
  os_free(client->connect_info.will_topic);
  len = CMD_ArgLen(&req);
  if (len > 128) return 0; // safety check
  client->connect_info.will_topic = (char*)os_zalloc(len + 1);
  CMD_PopArg(&req, client->connect_info.will_topic, len);
  client->connect_info.will_topic[len] = 0;

  // get message
  if (client->connect_info.will_message)
  os_free(client->connect_info.will_message);
  len = CMD_ArgLen(&req);
  // TODO: safety check
  client->connect_info.will_message = (char*)os_zalloc(len + 1);
  CMD_PopArg(&req, client->connect_info.will_message, len);
  client->connect_info.will_message[len] = 0;

  // get qos
  CMD_PopArg(&req, (uint8_t*)&client->connect_info.will_qos, 4);

  // get retain
  CMD_PopArg(&req, (uint8_t*)&client->connect_info.will_retain, 4);
#ifdef MQTTCMD_DBG
  os_printf("MQTT: MQTTCMD_Lwt topic=%s, message=%s, qos=%d, retain=%d\n",
       client->connect_info.will_topic,
       client->connect_info.will_message,
       client->connect_info.will_qos,
       client->connect_info.will_retain);
#endif
  return 1;
}

uint32_t ICACHE_FLASH_ATTR
MQTTCMD_Connect(CmdPacket *cmd) {
  CmdRequest req;
  CMD_Request(&req, cmd);

  if (CMD_GetArgc(&req) != 4)
    return 0;

  // get mqtt client
  uint32_t client_ptr;
  CMD_PopArg(&req, (uint8_t*)&client_ptr, 4);
  MQTT_Client* client = (MQTT_Client*)client_ptr;
#ifdef MQTTCMD_DBG
  os_printf("MQTT: MQTTCMD_Connect client ptr=%p\n", (void*)client_ptr);
#endif

  uint16_t len;

  // get host
  if (client->host)
  os_free(client->host);
  len = CMD_ArgLen(&req);
  if (len > 128) return 0; // safety check
  client->host = (char*)os_zalloc(len + 1);
  CMD_PopArg(&req, client->host, len);
  client->host[len] = 0;

  // get port
  CMD_PopArg(&req, (uint8_t*)&client->port, 4);

  // get security
  CMD_PopArg(&req, (uint8_t*)&client->security, 4);
#ifdef MQTTCMD_DBG
  os_printf("MQTT: MQTTCMD_Connect host=%s, port=%d, security=%d\n",
    client->host,
    client->port,
    client->security);
#endif

  MQTT_Connect(client);
  return 1;
}

uint32_t ICACHE_FLASH_ATTR
MQTTCMD_Disconnect(CmdPacket *cmd) {
  CmdRequest req;
  CMD_Request(&req, cmd);

  if (CMD_GetArgc(&req) != 1)
    return 0;

  // get mqtt client
  uint32_t client_ptr;
  CMD_PopArg(&req, (uint8_t*)&client_ptr, 4);
  MQTT_Client* client = (MQTT_Client*)client_ptr;
#ifdef MQTTCMD_DBG
  os_printf("MQTT: MQTTCMD_Disconnect client ptr=%p\n", (void*)client_ptr);
#endif

  // disconnect
  MQTT_Disconnect(client);
  return 1;
}

uint32_t ICACHE_FLASH_ATTR
MQTTCMD_Publish(CmdPacket *cmd) {
  CmdRequest req;
  CMD_Request(&req, cmd);

  if (CMD_GetArgc(&req) != 6)
    return 0;

  // get mqtt client
  uint32_t client_ptr;
  CMD_PopArg(&req, (uint8_t*)&client_ptr, 4);
  MQTT_Client* client = (MQTT_Client*)client_ptr;
#ifdef MQTTCMD_DBG
  os_printf("MQTT: MQTTCMD_Publish client ptr=%p\n", (void*)client_ptr);
#endif

  uint16_t len;
  uint8_t *topic, *data;
  uint32_t qos = 0, retain = 0, data_len;

  // get topic
  len = CMD_ArgLen(&req);
  if (len > 128) return 0; // safety check
  topic = (uint8_t*)os_zalloc(len + 1);
  CMD_PopArg(&req, topic, len);
  topic[len] = 0;

  // get data
  len = CMD_ArgLen(&req);
  // TODO: Safety check
  // TODO: this was orignially zalloc len not len+1
  data = (uint8_t*)os_zalloc(len+1);
  CMD_PopArg(&req, data, len);
  // TODO: next line not originally present
  data[len] = 0;

  // get data length
  // TODO: this isn't used but we have to pull it off the stack
  CMD_PopArg(&req, (uint8_t*)&data_len, 4);

  // get qos
  CMD_PopArg(&req, (uint8_t*)&qos, 4);

  // get retain
  CMD_PopArg(&req, (uint8_t*)&retain, 4);
#ifdef MQTTCMD_DBG
  os_printf("MQTT: MQTTCMD_Publish topic=%s, data_len=%d, qos=%ld, retain=%ld\n",
    topic,
    os_strlen((char*)data),
    qos,
    retain);
#endif

  MQTT_Publish(client, (char*)topic, (char*)data, (uint8_t)qos, (uint8_t)retain);
  os_free(topic);
  os_free(data);
  return 1;
}

uint32_t ICACHE_FLASH_ATTR
MQTTCMD_Subscribe(CmdPacket *cmd) {
  CmdRequest req;
  CMD_Request(&req, cmd);

  if (CMD_GetArgc(&req) != 3)
    return 0;

  // get mqtt client
  uint32_t client_ptr;
  CMD_PopArg(&req, (uint8_t*)&client_ptr, 4);
  MQTT_Client* client = (MQTT_Client*)client_ptr;
#ifdef MQTTCMD_DBG
  os_printf("MQTT: MQTTCMD_Subscribe client ptr=%p\n", (void*)client_ptr);
#endif

  uint16_t len;
  uint8_t* topic;
  uint32_t qos = 0;

  // get topic
  len = CMD_ArgLen(&req);
  if (len > 128) return 0; // safety check
  topic = (uint8_t*)os_zalloc(len + 1);
  CMD_PopArg(&req, topic, len);
  topic[len] = 0;

  // get qos
  CMD_PopArg(&req, (uint8_t*)&qos, 4);
#ifdef MQTTCMD_DBG
  os_printf("MQTT: MQTTCMD_Subscribe topic=%s, qos=%ld\n", topic, qos);
#endif
  MQTT_Subscribe(client, (char*)topic, (uint8_t)qos);
  os_free(topic);
  return 1;
}
