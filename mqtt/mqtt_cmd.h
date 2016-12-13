#ifndef MODULES_MQTT_CMD_H_
#define MODULES_MQTT_CMD_H_

#include "cmd.h"

typedef struct {
  uint32_t connectedCb;
  uint32_t disconnectedCb;
  uint32_t publishedCb;
  uint32_t dataCb;
} MqttCmdCb;

void MQTTCMD_Connect(CmdPacket *cmd);
void MQTTCMD_Disconnect(CmdPacket *cmd);
void MQTTCMD_Setup(CmdPacket *cmd);
void MQTTCMD_Publish(CmdPacket *cmd);
void MQTTCMD_Subscribe(CmdPacket *cmd);
void MQTTCMD_Lwt(CmdPacket *cmd);

void mqtt_block();
void mqtt_unblock();

#endif /* MODULES_MQTT_CMD_H_ */
