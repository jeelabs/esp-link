#ifndef MODULES_MQTT_CMD_H_
#define MODULES_MQTT_CMD_H_

#include "cmd.h"

typedef struct {
  uint32_t connectedCb;
  uint32_t disconnectedCb;
  uint32_t publishedCb;
  uint32_t dataCb;
} MqttCmdCb;

uint32_t MQTTCMD_Connect(CmdPacket *cmd);
uint32_t MQTTCMD_Disconnect(CmdPacket *cmd);
uint32_t MQTTCMD_Setup(CmdPacket *cmd);
uint32_t MQTTCMD_Publish(CmdPacket *cmd);
uint32_t MQTTCMD_Subscribe(CmdPacket *cmd);
uint32_t MQTTCMD_Lwt(CmdPacket *cmd);

#endif /* MODULES_MQTT_CMD_H_ */
