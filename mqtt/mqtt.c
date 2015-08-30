/* mqtt.c
*  Protocol: http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html
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

#include "mqtt.h"

#define MQTT_TASK_PRIO        		0
#define MQTT_TASK_QUEUE_SIZE    	1
#define MQTT_SEND_TIMOUT			    5

#ifndef QUEUE_BUFFER_SIZE
#define QUEUE_BUFFER_SIZE		 	2048
#endif

unsigned char* default_certificate;
unsigned int default_certificate_len = 0;
unsigned char* default_private_key;
unsigned int default_private_key_len = 0;

os_event_t mqtt_procTaskQueue[MQTT_TASK_QUEUE_SIZE];

LOCAL void ICACHE_FLASH_ATTR
mqtt_dns_found(const char* name, ip_addr_t* ipaddr, void* arg) {
  struct espconn* pConn = (struct espconn *)arg;
  MQTT_Client* client = (MQTT_Client *)pConn->reverse;


  if (ipaddr == NULL) {
    os_printf("DNS: Found, but got no ip, try to reconnect\n");
    client->connState = TCP_RECONNECT_REQ;
    return;
  }

  os_printf("DNS: found ip %d.%d.%d.%d\n",
            *((uint8 *)&ipaddr->addr),
            *((uint8 *)&ipaddr->addr + 1),
            *((uint8 *)&ipaddr->addr + 2),
            *((uint8 *)&ipaddr->addr + 3));

  if (client->ip.addr == 0 && ipaddr->addr != 0) {
    os_memcpy(client->pCon->proto.tcp->remote_ip, &ipaddr->addr, 4);
#ifdef CLIENT_SSL_ENABLE
    if (client->security){
      espconn_secure_connect(client->pCon);
    }
    else
#endif
    espconn_connect(client->pCon);

    client->connState = TCP_CONNECTING;
    os_printf("MQTT-TCP: connecting...\n");
  }

  system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
}


LOCAL void ICACHE_FLASH_ATTR
deliver_publish(MQTT_Client* client, uint8_t* message, uint16_t length) {
  mqtt_event_data_t event_data;

  event_data.topic_length = length;
  event_data.topic = mqtt_get_publish_topic(message, &event_data.topic_length);
  event_data.data_length = length;
  event_data.data = mqtt_get_publish_data(message, &event_data.data_length);

  if (client->dataCb)
    client->dataCb((uint32_t*)client, event_data.topic, event_data.topic_length, event_data.data, event_data.data_length);

  if (client->cmdDataCb)
    client->cmdDataCb((uint32_t*)client, event_data.topic, event_data.topic_length, event_data.data, event_data.data_length);

}

/**
* @brief  Client received callback function.
* @param  arg: contain the ip link information
* @param  pdata: received data
* @param  len: the lenght of received data
* @retval None
*/
void ICACHE_FLASH_ATTR
mqtt_tcpclient_recv(void* arg, char* pdata, unsigned short len) {
  uint8_t msg_type;
  uint8_t msg_qos;
  uint16_t msg_id;

  struct espconn* pCon = (struct espconn*)arg;
  MQTT_Client* client = (MQTT_Client *)pCon->reverse;

READPACKET:
  os_printf("MQTT-TCP: Data received %d bytes\n", len);
  if (len < MQTT_BUF_SIZE && len > 0) {
    os_memcpy(client->mqtt_state.in_buffer, pdata, len);

    msg_type = mqtt_get_type(client->mqtt_state.in_buffer);
    msg_qos = mqtt_get_qos(client->mqtt_state.in_buffer);
    msg_id = mqtt_get_id(client->mqtt_state.in_buffer, client->mqtt_state.in_buffer_length);
    if (client->connState == MQTT_CONNECT_SENDING) {
      if (msg_type == MQTT_MSG_TYPE_CONNACK) {
        if (client->mqtt_state.pending_msg_type != MQTT_MSG_TYPE_CONNECT) {
          os_printf("MQTT: Invalid packet\n");
#ifdef CLIENT_SSL_ENABLE
          if (client->security){
            espconn_secure_disconnect(client->pCon);
          }
          else
#endif
          espconn_disconnect(client->pCon);
        }
        else {
          os_printf("MQTT: Connected to %s:%ld\n", client->host, client->port);
          client->connState = MQTT_DATA;
          if (client->connectedCb)
            client->connectedCb((uint32_t*)client);
          if (client->cmdConnectedCb)
            client->cmdConnectedCb((uint32_t*)client);
        }
      }
    }
    else if (client->connState == MQTT_DATA) {
      client->mqtt_state.message_length_read = len;
      client->mqtt_state.message_length = mqtt_get_total_length(client->mqtt_state.in_buffer, client->mqtt_state.message_length_read);


      if (msg_type == MQTT_MSG_TYPE_SUBACK) {
        if (client->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_SUBSCRIBE && client->mqtt_state.pending_msg_id == msg_id)
        os_printf("MQTT: Subscribe successful\n");
      }
      else if (msg_type == MQTT_MSG_TYPE_UNSUBACK) {
        if (client->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_UNSUBSCRIBE && client->mqtt_state.pending_msg_id == msg_id)
        os_printf("MQTT: UnSubscribe successful\n");
      }
      else if (msg_type == MQTT_MSG_TYPE_PUBLISH) {
        if (msg_qos == 1)
          client->mqtt_state.outbound_message = mqtt_msg_puback(&client->mqtt_state.mqtt_connection, msg_id);
        else if (msg_qos == 2)
          client->mqtt_state.outbound_message = mqtt_msg_pubrec(&client->mqtt_state.mqtt_connection, msg_id);
        if (msg_qos == 1 || msg_qos == 2) {
          os_printf("MQTT: Queue response QoS: %d\n", msg_qos);
          if (QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1) {
            os_printf("MQTT: Queue full\n");
          }
        }
        deliver_publish(client, client->mqtt_state.in_buffer, client->mqtt_state.message_length_read);
      }
      else if (msg_type == MQTT_MSG_TYPE_PUBACK) {
        if (client->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_PUBLISH && client->mqtt_state.pending_msg_id == msg_id) {
          os_printf("MQTT: received MQTT_MSG_TYPE_PUBACK, finish QoS1 publish\n");
        }
      }
      else if (msg_type == MQTT_MSG_TYPE_PUBREC) {
        client->mqtt_state.outbound_message = mqtt_msg_pubrel(&client->mqtt_state.mqtt_connection, msg_id);
        if (QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1) {
          os_printf("MQTT: Queue full\n");
        }
      }
      else if (msg_type == MQTT_MSG_TYPE_PUBREL) {
        client->mqtt_state.outbound_message = mqtt_msg_pubcomp(&client->mqtt_state.mqtt_connection, msg_id);
        if (QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1) {
          os_printf("MQTT: Queue full\n");
        }
      }
      else if (msg_type == MQTT_MSG_TYPE_PUBCOMP) {
        if (client->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_PUBLISH && client->mqtt_state.pending_msg_id == msg_id) {
          os_printf("MQTT: receive MQTT_MSG_TYPE_PUBCOMP, finish QoS2 publish\n");
        }
      }
      else if (msg_type == MQTT_MSG_TYPE_PINGREQ) {
        client->mqtt_state.outbound_message = mqtt_msg_pingresp(&client->mqtt_state.mqtt_connection);
        if (QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1) {
          os_printf("MQTT: Queue full\n");
        }
      }

      // NOTE: this is done down here and not in the switch case above
      // because the PSOCK_READBUF_LEN() won't work inside a switch
      // statement due to the way protothreads resume.
      if (msg_type == MQTT_MSG_TYPE_PUBLISH) {
        len = client->mqtt_state.message_length_read;

        if (client->mqtt_state.message_length < client->mqtt_state.message_length_read) {
          //client->connState = MQTT_PUBLISH_RECV;
          //Not Implement yet
          len -= client->mqtt_state.message_length;
          pdata += client->mqtt_state.message_length;

          os_printf("Get another published message\n");
          goto READPACKET;
        }
      }
    }
  }
  else {
    os_printf("ERROR: Message too long\n");
  }
  system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
}

/**
* @brief  Client send over callback function.
* @param  arg: contain the ip link information
* @retval None
*/
void ICACHE_FLASH_ATTR
mqtt_tcpclient_sent_cb(void* arg) {
  struct espconn* pCon = (struct espconn *)arg;
  MQTT_Client* client = (MQTT_Client *)pCon->reverse;
  os_printf("MQTT-TCP: Sent\n");
  client->sendTimeout = 0;
  if (client->connState == MQTT_DATA && client->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_PUBLISH) {
    if (client->publishedCb)
      client->publishedCb((uint32_t*)client);
    if (client->cmdPublishedCb)
      client->cmdPublishedCb((uint32_t*)client);
  }
  system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
}

void ICACHE_FLASH_ATTR 
mqtt_timer(void* arg) {
  MQTT_Client* client = (MQTT_Client*)arg;

  if (client->connState == MQTT_DATA) {
    client->keepAliveTick++;
    if (client->keepAliveTick > client->mqtt_state.connect_info->keepalive) {

      os_printf("\nMQTT: Send keepalive packet to %s:%ld!\n", client->host, client->port);
      client->mqtt_state.outbound_message = mqtt_msg_pingreq(&client->mqtt_state.mqtt_connection);
      client->mqtt_state.pending_msg_type = MQTT_MSG_TYPE_PINGREQ;
      client->mqtt_state.pending_msg_type = mqtt_get_type(client->mqtt_state.outbound_message->data);
      client->mqtt_state.pending_msg_id = mqtt_get_id(client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);


      client->sendTimeout = MQTT_SEND_TIMOUT;
      os_printf("MQTT: Sending, type: %d, id: %04X\n", client->mqtt_state.pending_msg_type, client->mqtt_state.pending_msg_id);
#ifdef CLIENT_SSL_ENABLE
      if (client->security){
        espconn_secure_sent(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);
      }
      else
#endif
      espconn_sent(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);

      client->mqtt_state.outbound_message = NULL;

      client->keepAliveTick = 0;
      system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
    }

  }
  else if (client->connState == TCP_RECONNECT_REQ) {
    client->reconnectTick++;
    if (client->reconnectTick > MQTT_RECONNECT_TIMEOUT) {
      client->reconnectTick = 0;
      client->connState = TCP_RECONNECT;
      if (client->tcpDisconnectedCb)
        client->tcpDisconnectedCb((uint32_t*)client);
      if (client->cmdTcpDisconnectedCb)
        client->cmdTcpDisconnectedCb((uint32_t*)client);
      system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
    }
  }
  if (client->sendTimeout > 0)
    client->sendTimeout--;
}

void ICACHE_FLASH_ATTR
mqtt_tcpclient_discon_cb(void* arg) {

  struct espconn* pespconn = (struct espconn *)arg;
  MQTT_Client* client = (MQTT_Client *)pespconn->reverse;
  os_printf("MQTT-TCP: Disconnected callback\n");
  client->connState = TCP_RECONNECT_REQ;
  if (client->disconnectedCb)
    client->disconnectedCb((uint32_t*)client);
  if (client->cmdDisconnectedCb)
    client->cmdDisconnectedCb((uint32_t*)client);

  system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
}

/**
* @brief  Tcp client connect success callback function.
* @param  arg: contain the ip link information
* @retval None
*/
void ICACHE_FLASH_ATTR
mqtt_tcpclient_connect_cb(void* arg) {
  struct espconn* pCon = (struct espconn *)arg;
  MQTT_Client* client = (MQTT_Client *)pCon->reverse;

  espconn_regist_disconcb(client->pCon, mqtt_tcpclient_discon_cb);
  espconn_regist_recvcb(client->pCon, mqtt_tcpclient_recv);////////
  espconn_regist_sentcb(client->pCon, mqtt_tcpclient_sent_cb);///////
  os_printf("MQTT: Connected to broker %s:%ld\n", client->host, client->port);

  mqtt_msg_init(&client->mqtt_state.mqtt_connection, client->mqtt_state.out_buffer, client->mqtt_state.out_buffer_length);
  client->mqtt_state.outbound_message = mqtt_msg_connect(&client->mqtt_state.mqtt_connection, client->mqtt_state.connect_info);
  client->mqtt_state.pending_msg_type = mqtt_get_type(client->mqtt_state.outbound_message->data);
  client->mqtt_state.pending_msg_id = mqtt_get_id(client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);


  client->sendTimeout = MQTT_SEND_TIMOUT;
  os_printf("MQTT: Sending, type: %d, id: %04X\n", client->mqtt_state.pending_msg_type, client->mqtt_state.pending_msg_id);
#ifdef CLIENT_SSL_ENABLE
  if (client->security){
    espconn_secure_sent(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);
  }
  else
#endif
  espconn_sent(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);

  client->mqtt_state.outbound_message = NULL;
  client->connState = MQTT_CONNECT_SENDING;
  system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
}

/**
* @brief  Tcp client connect repeat callback function.
* @param  arg: contain the ip link information
* @retval None
*/
void ICACHE_FLASH_ATTR
mqtt_tcpclient_recon_cb(void* arg, int8_t errType) {
  struct espconn* pCon = (struct espconn *)arg;
  MQTT_Client* client = (MQTT_Client *)pCon->reverse;

  os_printf("MQTT-TCP: Reconnect to %s:%ld\n", client->host, client->port);

  client->connState = TCP_RECONNECT_REQ;

  system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
}

/**
* @brief  MQTT publish function.
* @param  client: 	MQTT_Client reference
* @param  topic: 		string topic will publish to
* @param  data: 		buffer data send point to
* @param  data_length: length of data
* @param  qos:		qos
* @param  retain:		retain
* @retval TRUE if success queue
*/
bool ICACHE_FLASH_ATTR
MQTT_Publish(MQTT_Client* client, const char* topic, const char* data, uint8_t qos, uint8_t retain) {
  int data_length = os_strlen(data);
  uint8_t dataBuffer[MQTT_BUF_SIZE];
  uint16_t dataLen;
  client->mqtt_state.outbound_message = mqtt_msg_publish(&client->mqtt_state.mqtt_connection,
                                                         topic, data, data_length,
                                                         qos, retain,
                                                         &client->mqtt_state.pending_msg_id);
  if (client->mqtt_state.outbound_message->length == 0) {
    os_printf("MQTT: Queuing Publish failed\n");
    return FALSE;
  }
  os_printf("MQTT: Queuing Publish, length: %d, queue size(%ld/%ld)\n", client->mqtt_state.outbound_message->length, client->msgQueue.rb.fill_cnt, client->msgQueue.rb.size);
  while (QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1) {
    os_printf("MQTT: Queue full\n");
    if (QUEUE_Gets(&client->msgQueue, dataBuffer, &dataLen, MQTT_BUF_SIZE) == -1) {
      os_printf("MQTT: Serious buffer error\n");
      return FALSE;
    }
  }
  system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
  return TRUE;
}

/**
* @brief  MQTT subscibe function.
* @param  client: 	MQTT_Client reference
* @param  topic: 		string topic will subscribe
* @param  qos:		qos
* @retval TRUE if success queue
*/
bool ICACHE_FLASH_ATTR
MQTT_Subscribe(MQTT_Client* client, char* topic, uint8_t qos) {
  uint8_t dataBuffer[MQTT_BUF_SIZE];
  uint16_t dataLen;

  client->mqtt_state.outbound_message = mqtt_msg_subscribe(&client->mqtt_state.mqtt_connection,
                                                           topic, 0,
                                                           &client->mqtt_state.pending_msg_id);
  os_printf("MQTT: Queue Subscribe, topic: \"%s\", id: %d\n", topic, client->mqtt_state.pending_msg_id);
  while (QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1) {
    os_printf("MQTT: Queue full\n");
    if (QUEUE_Gets(&client->msgQueue, dataBuffer, &dataLen, MQTT_BUF_SIZE) == -1) {
      os_printf("MQTT: Serious buffer error\n");
      return FALSE;
    }
  }
  system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
  return TRUE;
}

void ICACHE_FLASH_ATTR 
MQTT_Task(os_event_t* e) {
  MQTT_Client* client = (MQTT_Client*)e->par;
  uint8_t dataBuffer[MQTT_BUF_SIZE];
  uint16_t dataLen;
  if (e->par == 0)
    return;

  if (client->connState == TCP_RECONNECT_REQ) {
    return;
  }
  else if (client->connState == TCP_RECONNECT) {
    MQTT_Connect(client);
    os_printf("MQTT-TCP: Reconnect to: %s:%ld\n", client->host, client->port);
    client->connState = TCP_CONNECTING;
  }
  else if (client->connState == MQTT_DATA) {
    if (QUEUE_IsEmpty(&client->msgQueue) || client->sendTimeout != 0)
      return;

    if (QUEUE_Gets(&client->msgQueue, dataBuffer, &dataLen, MQTT_BUF_SIZE) == 0) {
      client->mqtt_state.pending_msg_type = mqtt_get_type(dataBuffer);
      client->mqtt_state.pending_msg_id = mqtt_get_id(dataBuffer, dataLen);
      client->sendTimeout = MQTT_SEND_TIMOUT;
      os_printf("MQTT: Sending, type: %d, id: %04X\n", client->mqtt_state.pending_msg_type, client->mqtt_state.pending_msg_id);
#ifdef CLIENT_SSL_ENABLE
      if (client->security){
        espconn_secure_sent(client->pCon, dataBuffer, dataLen);
      }
      else
#endif
      espconn_sent(client->pCon, dataBuffer, dataLen);

      client->mqtt_state.outbound_message = NULL;
      return;
    }
    return;
  }
}

/**
* @brief  MQTT initialization connection function
* @param  client: 	MQTT_Client reference
* @param  host: 	Domain or IP string
* @param  port: 	Port to connect
* @param  security:		1 for ssl, 0 for none
* @retval None
*/
void ICACHE_FLASH_ATTR
MQTT_InitConnection(MQTT_Client* mqttClient, char* host, uint32 port, uint8_t security) {
  os_printf("MQTT_InitConnection\n");
  uint8_t len = sizeof(MQTT_Client);
  os_memset(mqttClient, 0, len);

  uint32_t temp = os_strlen(host);
  mqttClient->host = (char*)os_zalloc(temp + 1);
  os_strcpy(mqttClient->host, host);
  mqttClient->host[temp] = 0;

  mqttClient->port = port;
  mqttClient->security = security;
}

/**
* @brief  MQTT initialization mqtt client function
* @param  client: 	MQTT_Client reference
* @param  clientid: 	MQTT client id
* @param  client_user:MQTT client user
* @param  client_pass:MQTT client password
* @param  client_pass:MQTT keep alive timer, in second
* @retval None
*/
void ICACHE_FLASH_ATTR
MQTT_InitClient(MQTT_Client* mqttClient, char* client_id, char* client_user, char* client_pass, uint8_t keepAliveTime, uint8_t cleanSession) {
  uint32_t temp;
  os_printf("MQTT_InitClient: ");

  os_memset(&mqttClient->connect_info, 0, sizeof(mqtt_connect_info_t));

  temp = os_strlen(client_id);
  mqttClient->connect_info.client_id = (char*)os_zalloc(temp + 1);
  os_strcpy(mqttClient->connect_info.client_id, client_id);
  mqttClient->connect_info.client_id[temp] = 0;

  temp = os_strlen(client_user);
  mqttClient->connect_info.username = (char*)os_zalloc(temp + 1);
  os_strcpy(mqttClient->connect_info.username, client_user);
  mqttClient->connect_info.username[temp] = 0;

  temp = os_strlen(client_pass);
  mqttClient->connect_info.password = (char*)os_zalloc(temp + 1);
  os_strcpy(mqttClient->connect_info.password, client_pass);
  mqttClient->connect_info.password[temp] = 0;

  mqttClient->connect_info.keepalive = keepAliveTime;
  mqttClient->connect_info.clean_session = cleanSession;

  mqttClient->mqtt_state.in_buffer = (uint8_t *)os_zalloc(MQTT_BUF_SIZE);
  mqttClient->mqtt_state.in_buffer_length = MQTT_BUF_SIZE;
  mqttClient->mqtt_state.out_buffer = (uint8_t *)os_zalloc(MQTT_BUF_SIZE);
  mqttClient->mqtt_state.out_buffer_length = MQTT_BUF_SIZE;
  mqttClient->mqtt_state.connect_info = &mqttClient->connect_info;

  mqtt_msg_init(&mqttClient->mqtt_state.mqtt_connection, mqttClient->mqtt_state.out_buffer, mqttClient->mqtt_state.out_buffer_length);

  QUEUE_Init(&mqttClient->msgQueue, QUEUE_BUFFER_SIZE);

  system_os_task(MQTT_Task, MQTT_TASK_PRIO, mqtt_procTaskQueue, MQTT_TASK_QUEUE_SIZE);
  system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)mqttClient);
  os_printf("client_id:%s, keepAlive:%d, cleanSession:%d\n",
    mqttClient->connect_info.client_id,
    mqttClient->connect_info.keepalive,
    mqttClient->connect_info.clean_session
  );
}

void ICACHE_FLASH_ATTR
MQTT_InitLWT(MQTT_Client* mqttClient, char* will_topic, char* will_msg, uint8_t will_qos, uint8_t will_retain) {
  uint32_t temp;
  temp = os_strlen((char*)will_topic);
  mqttClient->connect_info.will_topic = (char*)os_zalloc(temp + 1);
  os_strcpy((char*)mqttClient->connect_info.will_topic, (char*)will_topic);
  mqttClient->connect_info.will_topic[temp] = 0;

  temp = os_strlen((char*)will_msg);
  mqttClient->connect_info.will_message = (char*)os_zalloc(temp + 1);
  os_strcpy((char*)mqttClient->connect_info.will_message, (char*)will_msg);
  mqttClient->connect_info.will_message[temp] = 0;


  mqttClient->connect_info.will_qos = will_qos;
  mqttClient->connect_info.will_retain = will_retain;
}

/**
* @brief  Begin connect to MQTT broker
* @param  client: MQTT_Client reference
* @retval None
*/
void ICACHE_FLASH_ATTR
MQTT_Connect(MQTT_Client* mqttClient) {
  MQTT_Disconnect(mqttClient);
  mqttClient->pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
  mqttClient->pCon->type = ESPCONN_TCP;
  mqttClient->pCon->state = ESPCONN_NONE;
  mqttClient->pCon->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
  mqttClient->pCon->proto.tcp->local_port = espconn_port();
  mqttClient->pCon->proto.tcp->remote_port = mqttClient->port;
  mqttClient->pCon->reverse = mqttClient;
  espconn_regist_connectcb(mqttClient->pCon, mqtt_tcpclient_connect_cb);
  espconn_regist_reconcb(mqttClient->pCon, mqtt_tcpclient_recon_cb);

  mqttClient->keepAliveTick = 0;
  mqttClient->reconnectTick = 0;


  os_timer_disarm(&mqttClient->mqttTimer);
  os_timer_setfn(&mqttClient->mqttTimer, (os_timer_func_t *)mqtt_timer, mqttClient);
  os_timer_arm(&mqttClient->mqttTimer, 1000, 1);

  if (UTILS_StrToIP((const char *)mqttClient->host, (void*)&mqttClient->pCon->proto.tcp->remote_ip)) {
    os_printf("MQTT-TCP: Connect to ip  %s:%ld\n", mqttClient->host, mqttClient->port);
#ifdef CLIENT_SSL_ENABLE
    if (mqttClient->security){
      espconn_secure_connect(mqttClient->pCon);
    }
    else
#endif
    espconn_connect(mqttClient->pCon);
  }
  else {
    os_printf("MQTT-TCP: Connect to domain %s:%ld\n", mqttClient->host, mqttClient->port);
    espconn_gethostbyname(mqttClient->pCon, (const char *)mqttClient->host, &mqttClient->ip, mqtt_dns_found);
  }
  mqttClient->connState = TCP_CONNECTING;
}

void ICACHE_FLASH_ATTR
MQTT_Disconnect(MQTT_Client* mqttClient) {
  if (mqttClient->pCon) {
    os_printf("Free memory\n");
    if (mqttClient->pCon->proto.tcp)
    os_free(mqttClient->pCon->proto.tcp);
    os_free(mqttClient->pCon);
    mqttClient->pCon = NULL;
  }

  os_timer_disarm(&mqttClient->mqttTimer);
}

void ICACHE_FLASH_ATTR
MQTT_OnConnected(MQTT_Client* mqttClient, MqttCallback connectedCb) {
  mqttClient->connectedCb = connectedCb;
}

void ICACHE_FLASH_ATTR
MQTT_OnDisconnected(MQTT_Client* mqttClient, MqttCallback disconnectedCb) {
  mqttClient->disconnectedCb = disconnectedCb;
}

void ICACHE_FLASH_ATTR
MQTT_OnTcpDisconnected(MQTT_Client *mqttClient, MqttCallback tcpDisconnectedCb)
{
  mqttClient->tcpDisconnectedCb = tcpDisconnectedCb;
}

void ICACHE_FLASH_ATTR
MQTT_OnData(MQTT_Client* mqttClient, MqttDataCallback dataCb) {
  mqttClient->dataCb = dataCb;
}

void ICACHE_FLASH_ATTR
MQTT_OnPublished(MQTT_Client* mqttClient, MqttCallback publishedCb) {
  mqttClient->publishedCb = publishedCb;
}
