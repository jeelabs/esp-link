#ifndef MODULES_MQTT_CMD_H_
#define MODULES_MQTT_CMD_H_

#include "cmd.h"
#include "mqtt.h"

typedef struct {
  uint32_t connectedCb;
  uint32_t disconnectedCb;
  uint32_t publishedCb;
  uint32_t dataCb;
  uint32_t tcpDisconnectedCb;
} MqttCmdCb;

uint32_t ICACHE_FLASH_ATTR MQTTCMD_Connect(CmdPacket *cmd);
uint32_t ICACHE_FLASH_ATTR MQTTCMD_Disconnect(CmdPacket *cmd);
uint32_t ICACHE_FLASH_ATTR MQTTCMD_Setup(CmdPacket *cmd);
uint32_t ICACHE_FLASH_ATTR MQTTCMD_Publish(CmdPacket *cmd);
uint32_t ICACHE_FLASH_ATTR MQTTCMD_Subscribe(CmdPacket *cmd);
uint32_t ICACHE_FLASH_ATTR MQTTCMD_Lwt(CmdPacket *cmd);

#endif /* MODULES_MQTT_CMD_H_ */
