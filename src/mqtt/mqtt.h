/* mqtt.h
*
* Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * Neither the name of Redis nor the names of its contributors may be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef MQTT_H_
#define MQTT_H_

#include "mqtt_msg.h"
#include "pktbuf.h"

// in rest.c
uint8_t UTILS_StrToIP(const char* str, void *ip);

// State of MQTT connection
typedef enum {
  MQTT_DISCONNECTED,    // we're in disconnected state
  TCP_RECONNECT_REQ,    // connect failed, needs reconnecting
  TCP_CONNECTING,       // in TCP connection process
  MQTT_CONNECTED,       // conneted (or connecting)
} tConnState;

typedef struct MQTT_Client MQTT_Client; // forward definition

// Simple notification callback
typedef void (*MqttCallback)(MQTT_Client *client);
// Callback with data messge
typedef void (*MqttDataCallback)(MQTT_Client *client, const char* topic, uint32_t topic_len,
    const char* data, uint32_t data_len);

// MQTTY client data structure
struct MQTT_Client {
  struct espconn*     pCon;                   // socket
  // connection information
  char*               host;                   // MQTT server
  uint16_t            port;
  uint8_t             security;               // 0=tcp, 1=ssl
  ip_addr_t           ip;                     // MQTT server IP address
  mqtt_connect_info_t connect_info;           // info to connect/reconnect
  // protocol state and message assembly
  tConnState          connState;              // connection state
  bool                sending;                // espconn_send is pending
  mqtt_connection_t   mqtt_connection;        // message assembly descriptor
  PktBuf*             msgQueue;               // queued outbound messages
  // TCP input buffer
  uint8_t*            in_buffer;
  int                 in_buffer_size;         // length allocated
  int                 in_buffer_filled;       // number of bytes held
  // outstanding message when we expect an ACK
  PktBuf*             pending_buffer;         // buffer sent and awaiting ACK
  PktBuf*             sending_buffer;         // buffer sent not awaiting ACK
  // timer and associated timeout counters
  ETSTimer            mqttTimer;              // timer for this connection
  uint8_t             keepAliveTick;          // seconds 'til keep-alive is required (0=no k-a)
  uint8_t             keepAliveAckTick;       // seconds 'til keep-alive ack is overdue (0=no k-a)
  uint8_t             timeoutTick;            // seconds 'til other timeout
  uint8_t             sendTimeout;            // value of send timeout setting
  uint8_t             reconTimeout;           // timeout to reconnect (back-off)
  // callbacks
  MqttCallback        connectedCb;
  MqttCallback        cmdConnectedCb;
  MqttCallback        disconnectedCb;
  MqttCallback        cmdDisconnectedCb;
  MqttCallback        publishedCb;
  MqttCallback        cmdPublishedCb;
  MqttDataCallback    dataCb;
  MqttDataCallback    cmdDataCb;
  // misc
  void*               user_data;
};

// Initialize client data structure
void MQTT_Init(MQTT_Client* mqttClient, char* host, uint32 port,
    uint8_t security, uint8_t sendTimeout,
    char* client_id, char* client_user, char* client_pass,
    uint8_t keepAliveTime);

// Completely free buffers associated with client data structure
// This does not free the mqttClient struct itself, it just readies the struct so
// it can be freed or MQTT_Init can be called on it again
void MQTT_Free(MQTT_Client* mqttClient);

// Set Last Will Topic on client, must be called before MQTT_InitConnection
void MQTT_InitLWT(MQTT_Client* mqttClient, char* will_topic, char* will_msg,
    uint8_t will_qos, uint8_t will_retain);

// Disconnect and reconnect in order to change params (such as LWT)
void MQTT_Reconnect(MQTT_Client* mqttClient);

// Kick of a persistent connection to the broker, will reconnect anytime conn breaks
void MQTT_Connect(MQTT_Client* mqttClient);

// Kill persistent connection
void MQTT_Disconnect(MQTT_Client* mqttClient);

// Subscribe to a topic
bool MQTT_Subscribe(MQTT_Client* client, char* topic, uint8_t qos);

// Publish a message
bool MQTT_Publish(MQTT_Client* client, const char* topic, const char* data, uint16_t data_len,
    uint8_t qos, uint8_t retain);

// Callback when connected
void MQTT_OnConnected(MQTT_Client* mqttClient, MqttCallback connectedCb);
// Callback when disconnected
void MQTT_OnDisconnected(MQTT_Client* mqttClient, MqttCallback disconnectedCb);
// Callback when publish succeeded
void MQTT_OnPublished(MQTT_Client* mqttClient, MqttCallback publishedCb);
// Callback when data arrives for subscription
void MQTT_OnData(MQTT_Client* mqttClient, MqttDataCallback dataCb);

#endif /* USER_AT_MQTT_H_ */
