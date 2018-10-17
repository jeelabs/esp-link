/* mqtt.c
*  Protocol: http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html
*
* Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
* All rights reserved.
*
* Modified by Thorsten von Eicken to make it fully callback based
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

// TODO:
// Handle SessionPresent=0 in CONNACK and rexmit subscriptions
// Improve timeout for CONNACK, currently only has keep-alive timeout (maybe send artificial ping?)
// Allow messages that don't require ACK to be sent even when pending_buffer is != NULL
// Set dup flag in retransmissions

#include <esp8266.h>
#include "pktbuf.h"
#include "mqtt.h"

#ifdef MQTT_DBG
#define DBG_MQTT(format, ...) os_printf(format, ## __VA_ARGS__)
#else
#define DBG_MQTT(format, ...) do { } while(0)
#endif

extern void dumpMem(void *buf, int len);

// HACK
sint8 espconn_secure_connect(struct espconn *espconn) {
  return espconn_connect(espconn);
}
sint8 espconn_secure_disconnect(struct espconn *espconn) {
  return espconn_disconnect(espconn);
}
sint8 espconn_secure_sent(struct espconn *espconn, uint8 *psent, uint16 length) {
  return espconn_sent(espconn, psent, length);
}

// max message size supported for receive
#define MQTT_MAX_RCV_MESSAGE 2048
// max message size for sending (except publish)
#define MQTT_MAX_SHORT_MESSAGE 128

#ifdef MQTT_DBG
static char* mqtt_msg_type[] = {
  "NULL", "TYPE_CONNECT", "CONNACK", "PUBLISH", "PUBACK", "PUBREC", "PUBREL", "PUBCOMP",
  "SUBSCRIBE", "SUBACK", "UNSUBSCRIBE", "UNSUBACK", "PINGREQ", "PINGRESP", "DISCONNECT", "RESV",
};
#endif

// forward declarations
static void mqtt_enq_message(MQTT_Client *client, const uint8_t *data, uint16_t len);
static void mqtt_send_message(MQTT_Client* client);
static void mqtt_doAbort(MQTT_Client* client);

// Deliver a publish message to the client
static void ICACHE_FLASH_ATTR
deliver_publish(MQTT_Client* client, uint8_t* message, uint16_t length) {

  // parse the message into topic and data
  uint16_t topic_length = length;
  const char *topic = mqtt_get_publish_topic(message, &topic_length);
  uint16_t data_length = length;
  const char *data = mqtt_get_publish_data(message, &data_length);

  // callback to client
  if (client->dataCb)
    client->dataCb(client, topic, topic_length, data, data_length);
  if (client->cmdDataCb)
    client->cmdDataCb(client, topic, topic_length, data, data_length);
}

/**
* @brief  Client received callback function.
* @param  arg: contain the ip link information
* @param  pdata: received data
* @param  len: the length of received data
* @retval None
*/
static void ICACHE_FLASH_ATTR
mqtt_tcpclient_recv(void* arg, char* pdata, unsigned short len) {
  //os_printf("MQTT: recv CB\n");
  uint8_t msg_type;
  uint16_t msg_id;
  uint16_t msg_len;

  struct espconn* pCon = (struct espconn*)arg;
  MQTT_Client* client = (MQTT_Client *)pCon->reverse;
  if (client == NULL) return; // aborted connection

  //os_printf("MQTT: Data received %d bytes\n", len);

  do {
    // append data to our buffer
    int avail = client->in_buffer_size - client->in_buffer_filled;
    if (len <= avail) {
      os_memcpy(client->in_buffer + client->in_buffer_filled, pdata, len);
      client->in_buffer_filled += len;
      len = 0;
    } else {
      os_memcpy(client->in_buffer + client->in_buffer_filled, pdata, avail);
      client->in_buffer_filled += avail;
      len -= avail;
      pdata += avail;
    }

    // check out what's at the head of the buffer
    msg_type = mqtt_get_type(client->in_buffer);
    msg_id = mqtt_get_id(client->in_buffer, client->in_buffer_size);
    msg_len = mqtt_get_total_length(client->in_buffer, client->in_buffer_size);

    if (msg_len > client->in_buffer_size) {
      // oops, too long a message for us to digest, disconnect and hope for a miracle
      os_printf("MQTT: Too long a message (%d bytes)\n", msg_len);
      mqtt_doAbort(client);
      return;
    }

    // check whether what's left in the buffer is a complete message
    if (msg_len > client->in_buffer_filled) break;

    if (client->connState != MQTT_CONNECTED) {
      // why are we receiving something??
      DBG_MQTT("MQTT ERROR: recv in invalid state %d\n", client->connState);
      mqtt_doAbort(client);
      return;
    }

    // we are connected and are sending/receiving data messages
    uint8_t pending_msg_type = 0;
    uint16_t pending_msg_id = 0;
    if (client->pending_buffer != NULL) {
      pending_msg_type = mqtt_get_type(client->pending_buffer->data);
      pending_msg_id = mqtt_get_id(client->pending_buffer->data, client->pending_buffer->filled);
    }
    DBG_MQTT("MQTT: Recv type=%s id=%04X len=%d; Pend type=%s id=%02X\n",
        mqtt_msg_type[msg_type], msg_id, msg_len, mqtt_msg_type[pending_msg_type],pending_msg_id);

    switch (msg_type) {
    case MQTT_MSG_TYPE_CONNACK:
      //DBG_MQTT("MQTT: Connect successful\n");
      // callbacks for internal and external clients
      if (client->connectedCb) client->connectedCb(client);
      if (client->cmdConnectedCb) client->cmdConnectedCb(client);
      client->reconTimeout = 1; // reset the reconnect backoff
      break;

    case MQTT_MSG_TYPE_SUBACK:
      if (pending_msg_type == MQTT_MSG_TYPE_SUBSCRIBE && pending_msg_id == msg_id) {
        //DBG_MQTT("MQTT: Subscribe successful\n");
        client->pending_buffer = PktBuf_ShiftFree(client->pending_buffer);
      }
      break;

    case MQTT_MSG_TYPE_UNSUBACK:
      if (pending_msg_type == MQTT_MSG_TYPE_UNSUBSCRIBE && pending_msg_id == msg_id) {
        //DBG_MQTT("MQTT: Unsubscribe successful\n");
        client->pending_buffer = PktBuf_ShiftFree(client->pending_buffer);
      }
      break;

    case MQTT_MSG_TYPE_PUBACK: // ack for a publish we sent
      if (pending_msg_type == MQTT_MSG_TYPE_PUBLISH && pending_msg_id == msg_id) {
        //DBG_MQTT("MQTT: QoS1 Publish successful\n");
        client->pending_buffer = PktBuf_ShiftFree(client->pending_buffer);
      }
      break;

    case MQTT_MSG_TYPE_PUBREC: // rec for a publish we sent
      if (pending_msg_type == MQTT_MSG_TYPE_PUBLISH && pending_msg_id == msg_id) {
        //DBG_MQTT("MQTT: QoS2 publish cont\n");
        client->pending_buffer = PktBuf_ShiftFree(client->pending_buffer);
        // we need to send PUBREL
        mqtt_msg_pubrel(&client->mqtt_connection, msg_id);
        mqtt_enq_message(client, client->mqtt_connection.message.data,
            client->mqtt_connection.message.length);
      }
      break;

    case MQTT_MSG_TYPE_PUBCOMP: // comp for a pubrel we sent (originally publish we sent)
      if (pending_msg_type == MQTT_MSG_TYPE_PUBREL && pending_msg_id == msg_id) {
        //DBG_MQTT("MQTT: QoS2 Publish successful\n");
        client->pending_buffer = PktBuf_ShiftFree(client->pending_buffer);
      }
      break;

    case MQTT_MSG_TYPE_PUBLISH: { // incoming publish
        // we may need to ACK the publish
        uint8_t msg_qos = mqtt_get_qos(client->in_buffer);
#ifdef MQTT_DBG
        uint16_t topic_length = msg_len;
        os_printf("MQTT: Recv PUBLISH qos=%d %s\n", msg_qos,
            mqtt_get_publish_topic(client->in_buffer, &topic_length));
#endif
        if (msg_qos == 1) mqtt_msg_puback(&client->mqtt_connection, msg_id);
        if (msg_qos == 2) mqtt_msg_pubrec(&client->mqtt_connection, msg_id);
        if (msg_qos == 1 || msg_qos == 2) {
          mqtt_enq_message(client, client->mqtt_connection.message.data,
              client->mqtt_connection.message.length);
        }
        // send the publish message to clients
        deliver_publish(client, client->in_buffer, msg_len);
      }
      break;

    case MQTT_MSG_TYPE_PUBREL: // rel for a rec we sent (originally publish received)
      if (pending_msg_type == MQTT_MSG_TYPE_PUBREC && pending_msg_id == msg_id) {
        //DBG_MQTT("MQTT: Cont QoS2 recv\n");
        client->pending_buffer = PktBuf_ShiftFree(client->pending_buffer);
        // we need to send PUBCOMP
        mqtt_msg_pubcomp(&client->mqtt_connection, msg_id);
        mqtt_enq_message(client, client->mqtt_connection.message.data,
            client->mqtt_connection.message.length);
      }
      break;

    case MQTT_MSG_TYPE_PINGRESP:
      client->keepAliveAckTick = 0;
      break;
    }

    // Shift out the message and see whether we have another one
    if (msg_len < client->in_buffer_filled)
      os_memcpy(client->in_buffer, client->in_buffer+msg_len, client->in_buffer_filled-msg_len);
    client->in_buffer_filled -= msg_len;
  } while(client->in_buffer_filled > 0 || len > 0);

  // Send next packet out, if possible
  if (!client->sending && client->pending_buffer == NULL && client->msgQueue != NULL) {
    mqtt_send_message(client);
  }
}

/**
* @brief  Callback from TCP that previous send completed
* @param  arg: contain the ip link information
* @retval None
*/
static void ICACHE_FLASH_ATTR
mqtt_tcpclient_sent_cb(void* arg) {
  //DBG_MQTT("MQTT: sent CB\n");
  struct espconn* pCon = (struct espconn *)arg;
  MQTT_Client* client = (MQTT_Client *)pCon->reverse;
  if (client == NULL) return; // aborted connection ?
  //DBG_MQTT("MQTT: Sent\n");

  // if the message we sent is not a "pending" one, we need to free the buffer
  if (client->sending_buffer != NULL) {
    PktBuf *buf = client->sending_buffer;
    //DBG_MQTT("PktBuf free %p l=%d\n", buf, buf->filled);
    os_free(buf);
    client->sending_buffer = NULL;
  }
  client->sending = false;

  // send next message if one is queued and we're not expecting an ACK
  if (client->connState == MQTT_CONNECTED && client->pending_buffer == NULL &&
      client->msgQueue != NULL) {
    mqtt_send_message(client);
  }
}

/*
 * @brief: Timer function to handle timeouts
 */
static void ICACHE_FLASH_ATTR
mqtt_timer(void* arg) {
  MQTT_Client* client = (MQTT_Client*)arg;
  //DBG_MQTT("MQTT: timer CB\n");

  switch (client->connState) {
  default: break;

  case MQTT_CONNECTED:
    // first check whether we're timing out for an ACK
    if (client->pending_buffer != NULL && --client->timeoutTick == 0) {
      // looks like we're not getting a response in time, abort the connection
      mqtt_doAbort(client);
      client->timeoutTick = 0; // trick to make reconnect happen in 1 second
      return;
    }

    // check whether our last keep-alive timed out
    if (client->keepAliveAckTick > 0 && --client->keepAliveAckTick == 0) {
      os_printf("\nMQTT ERROR: Keep-alive timed out\n");
      mqtt_doAbort(client);
      return;
    }

    // check whether we need to send a keep-alive message
    if (client->keepAliveTick > 0 && --client->keepAliveTick == 0) {
      // timeout: we need to send a ping message
      //DBG_MQTT("MQTT: Send keepalive\n");
      mqtt_msg_pingreq(&client->mqtt_connection);
      PktBuf *buf = PktBuf_New(client->mqtt_connection.message.length);
      os_memcpy(buf->data, client->mqtt_connection.message.data,
          client->mqtt_connection.message.length);
      buf->filled = client->mqtt_connection.message.length;
      client->msgQueue = PktBuf_Unshift(client->msgQueue, buf);
      mqtt_send_message(client);
      client->keepAliveTick = client->connect_info.keepalive;
      client->keepAliveAckTick = client->sendTimeout;
    }

    break;

  case TCP_RECONNECT_REQ:
    if (client->timeoutTick == 0 || --client->timeoutTick == 0) {
      // it's time to reconnect! start by re-enqueueing anything pending
      if (client->pending_buffer != NULL) {
        client->msgQueue = PktBuf_Unshift(client->msgQueue, client->pending_buffer);
        client->pending_buffer = NULL;
      }
      client->connect_info.clean_session = 0; // ask server to keep state
      MQTT_Connect(client);
    }
  }
}

/**
 * @brief  Callback from SDK that socket is disconnected
 * @param  arg: contain the ip link information
 * @retval None
 */
void ICACHE_FLASH_ATTR
mqtt_tcpclient_discon_cb(void* arg) {
  struct espconn* pespconn = (struct espconn *)arg;
  MQTT_Client* client = (MQTT_Client *)pespconn->reverse;
  DBG_MQTT("MQTT: Disconnect CB, freeing espconn %p\n", arg);
  if (pespconn->proto.tcp) os_free(pespconn->proto.tcp);
  os_free(pespconn);

  // if this is an aborted connection we're done
  if (client == NULL) return;
  DBG_MQTT("MQTT: Disconnected from %s:%d\n", client->host, client->port);
  if (client->disconnectedCb) client->disconnectedCb(client);
  if (client->cmdDisconnectedCb) client->cmdDisconnectedCb(client);

  // reconnect unless we're in a permanently disconnected state
  if (client->connState == MQTT_DISCONNECTED) return;
  client->timeoutTick = client->reconTimeout;
  if (client->reconTimeout < 128) client->reconTimeout <<= 1;
  client->connState = TCP_RECONNECT_REQ;
}

/**
* @brief  Callback from SDK that socket got reset, note that no discon_cb will follow
* @param  arg: contain the ip link information
* @retval None
*/
static void ICACHE_FLASH_ATTR
mqtt_tcpclient_recon_cb(void* arg, int8_t err) {
  struct espconn* pespconn = (struct espconn *)arg;
  MQTT_Client* client = (MQTT_Client *)pespconn->reverse;
  //DBG_MQTT("MQTT: Reset CB, freeing espconn %p (err=%d)\n", arg, err);
  if (pespconn->proto.tcp) os_free(pespconn->proto.tcp);
  os_free(pespconn);
  os_printf("MQTT: Connection reset from %s:%d\n", client->host, client->port);
  if (client->disconnectedCb) client->disconnectedCb(client);
  if (client->cmdDisconnectedCb) client->cmdDisconnectedCb(client);

  // reconnect unless we're in a permanently disconnected state
  if (client->connState == MQTT_DISCONNECTED) return;
  client->timeoutTick = client->reconTimeout;
  if (client->reconTimeout < 128) client->reconTimeout <<= 1;
  client->connState = TCP_RECONNECT_REQ;
  os_printf("timeoutTick=%d reconTimeout=%d\n", client->timeoutTick, client->reconTimeout);
}


/**
* @brief  Callback from SDK that socket is connected
* @param  arg: contain the ip link information
* @retval None
*/
static void ICACHE_FLASH_ATTR
mqtt_tcpclient_connect_cb(void* arg) {
  struct espconn* pCon = (struct espconn *)arg;
  MQTT_Client* client = (MQTT_Client *)pCon->reverse;
  if (client == NULL) return; // aborted connection

  espconn_regist_disconcb(client->pCon, mqtt_tcpclient_discon_cb);
  espconn_regist_recvcb(client->pCon, mqtt_tcpclient_recv);
  espconn_regist_sentcb(client->pCon, mqtt_tcpclient_sent_cb);
  os_printf("MQTT: TCP connected to %s:%d\n", client->host, client->port);

  // send MQTT connect message to broker
  mqtt_msg_connect(&client->mqtt_connection, &client->connect_info);
  PktBuf *buf = PktBuf_New(client->mqtt_connection.message.length);
  os_memcpy(buf->data, client->mqtt_connection.message.data,
      client->mqtt_connection.message.length);
  buf->filled = client->mqtt_connection.message.length;
  client->msgQueue = PktBuf_Unshift(client->msgQueue, buf); // prepend to send (rexmit) queue
  mqtt_send_message(client);
  client->connState = MQTT_CONNECTED; // v3.1.1 allows publishing while still connecting
}

/**
 * @brief  Allocate and enqueue mqtt message, kick sending, if appropriate
 */
static void ICACHE_FLASH_ATTR
mqtt_enq_message(MQTT_Client *client, const uint8_t *data, uint16_t len) {
  PktBuf *buf = PktBuf_New(len);
  os_memcpy(buf->data, data, len);
  buf->filled = len;
  client->msgQueue = PktBuf_Push(client->msgQueue, buf);

  if (client->connState == MQTT_CONNECTED && !client->sending && client->pending_buffer == NULL) {
    mqtt_send_message(client);
  }
}

/**
 * @brief  Send out top message in queue onto socket
 */
static void ICACHE_FLASH_ATTR
mqtt_send_message(MQTT_Client* client) {
  //DBG_MQTT("MQTT: Send_message\n");
  PktBuf *buf = client->msgQueue;
  if (buf == NULL || client->sending) return; // ahem...
  client->msgQueue = PktBuf_Shift(client->msgQueue);

  // get some details about the message
  uint16_t msg_type = mqtt_get_type(buf->data);
  uint8_t  msg_id = mqtt_get_id(buf->data, buf->filled);
#ifdef MQTT_DBG
  os_printf("MQTT: Send type=%s id=%04X len=%d\n", mqtt_msg_type[msg_type], msg_id, buf->filled);
#if 0
  for (int i=0; i<buf->filled; i++) {
    if (buf->data[i] >= ' ' && buf->data[i] <= '~') os_printf("%c", buf->data[i]);
    else os_printf("\\x%02X", buf->data[i]);
  }
  os_printf("\n");
#endif
#endif

  // send the message out
  if (client->security)
    espconn_secure_sent(client->pCon, buf->data, buf->filled);
  else
    espconn_sent(client->pCon, buf->data, buf->filled);
  client->sending = true;

  // depending on whether it needs an ack we need to hold on to the message
  bool needsAck =
    (msg_type == MQTT_MSG_TYPE_PUBLISH && mqtt_get_qos(buf->data) > 0) ||
    msg_type == MQTT_MSG_TYPE_PUBREL || msg_type == MQTT_MSG_TYPE_PUBREC ||
    msg_type == MQTT_MSG_TYPE_SUBSCRIBE || msg_type == MQTT_MSG_TYPE_UNSUBSCRIBE ||
    msg_type == MQTT_MSG_TYPE_PINGREQ;
  if (msg_type == MQTT_MSG_TYPE_PINGREQ) {
    client->pending_buffer = NULL; // we don't need to rexmit this one
    client->sending_buffer = buf;
  } else if (needsAck) {
    client->pending_buffer = buf;  // remeber for rexmit on disconnect/reconnect
    client->sending_buffer = NULL;
    client->timeoutTick = client->sendTimeout+1; // +1 to ensure full sendTireout seconds
  } else {
    client->pending_buffer = NULL;
    client->sending_buffer = buf;
    client->timeoutTick = 0;
  }
  client->keepAliveTick = client->connect_info.keepalive > 0 ? client->connect_info.keepalive+1 : 0;
}

/**
* @brief  DNS lookup for broker hostname completed, move to next phase
*/
static void ICACHE_FLASH_ATTR
mqtt_dns_found(const char* name, ip_addr_t* ipaddr, void* arg) {
  struct espconn* pConn = (struct espconn *)arg;
  MQTT_Client* client = (MQTT_Client *)pConn->reverse;

  if (ipaddr == NULL) {
    os_printf("MQTT: DNS lookup failed\n");
    if (client != NULL) {
      client->timeoutTick = client->reconTimeout;
      if (client->reconTimeout < 128) client->reconTimeout <<= 1;
      client->connState = TCP_RECONNECT_REQ; // the timer will kick-off a reconnection
    }
    return;
  }
  DBG_MQTT("MQTT: ip %d.%d.%d.%d\n",
            *((uint8 *)&ipaddr->addr),
            *((uint8 *)&ipaddr->addr + 1),
            *((uint8 *)&ipaddr->addr + 2),
            *((uint8 *)&ipaddr->addr + 3));

  if (client != NULL && client->ip.addr == 0 && ipaddr->addr != 0) {
    os_memcpy(client->pCon->proto.tcp->remote_ip, &ipaddr->addr, 4);
    uint8_t err;
    if (client->security)
      err = espconn_secure_connect(client->pCon);
    else
      err = espconn_connect(client->pCon);
    if (err != 0) {
      os_printf("MQTT ERROR: Failed to connect\n");
      client->timeoutTick = client->reconTimeout;
      if (client->reconTimeout < 128) client->reconTimeout <<= 1;
      client->connState = TCP_RECONNECT_REQ;
    } else {
      DBG_MQTT("MQTT: connecting...\n");
    }
  }
}

//===== publish / subscribe

static void ICACHE_FLASH_ATTR
msg_conn_init(mqtt_connection_t *new_msg, mqtt_connection_t *old_msg,
    uint8_t *buf, uint16_t buflen) {
  new_msg->message_id = old_msg->message_id;
  new_msg->buffer = buf;
  new_msg->buffer_length = buflen;
}

/**
* @brief  MQTT publish function.
* @param  client: MQTT_Client reference
* @param  topic:  string topic will publish to
* @param  data:   buffer data send point to
* @param  data_length: length of data
* @param  qos:    qos
* @param  retain: retain
* @retval TRUE if success queue
*/
bool ICACHE_FLASH_ATTR
MQTT_Publish(MQTT_Client* client, const char* topic, const char* data, uint16_t data_length,
    uint8_t qos, uint8_t retain)
{
  // estimate the packet size to allocate a buffer
  uint16_t topic_length = os_strlen(topic);
  // estimate: fixed hdr, pkt-id, topic length, topic, data, fudge
  uint16_t buf_len = 3 + 2 + 2 + topic_length + data_length + 16;
  PktBuf *buf = PktBuf_New(buf_len);
  if (buf == NULL) {
    os_printf("MQTT ERROR: Cannot allocate buffer for %d byte publish\n", buf_len);
    return FALSE;
  }
  // use a temporary mqtt_message_t pointing to our buffer, this is a bit of a mess because we
  // need to keep track of the message_id that is embedded in it
  mqtt_connection_t msg;
  msg_conn_init(&msg, &client->mqtt_connection, buf->data, buf_len);
  uint16_t msg_id;
  if (!mqtt_msg_publish(&msg, topic, data, data_length, qos, retain, &msg_id)){
    os_printf("MQTT ERROR: Queuing Publish failed\n");
    os_free(buf);
    return FALSE;
  }
  client->mqtt_connection.message_id = msg.message_id;
  if (msg.message.data != buf->data)
    os_memcpy(buf->data, msg.message.data, msg.message.length);
  buf->filled = msg.message.length;

  DBG_MQTT("MQTT: Publish, topic: \"%s\", length: %d\n", topic, msg.message.length);
  //dumpMem(buf, buf_len);
  client->msgQueue = PktBuf_Push(client->msgQueue, buf);

  if (!client->sending && client->pending_buffer == NULL) {
    mqtt_send_message(client);
  }
  return TRUE;
}

/**
* @brief  MQTT subscribe function.
* @param  client: MQTT_Client reference
* @param  topic:  string topic will subscribe
* @param  qos:    qos
* @retval TRUE if success queue
*/
bool ICACHE_FLASH_ATTR
MQTT_Subscribe(MQTT_Client* client, char* topic, uint8_t qos) {
  uint16_t msg_id;
  if (!mqtt_msg_subscribe(&client->mqtt_connection, topic, 0, &msg_id)) {
    os_printf("MQTT ERROR: Queuing Subscribe failed (too long)\n");
    return FALSE;
  }
  DBG_MQTT("MQTT: Subscribe, topic: \"%s\"\n", topic);
  mqtt_enq_message(client, client->mqtt_connection.message.data,
      client->mqtt_connection.message.length);
  return TRUE;
}

//===== Initialization and connect/disconnect

/**
* @brief  MQTT initialization mqtt client function
* @param  client:        MQTT_Client reference
* @param  host:   Domain or IP string
* @param  port:   Port to connect
* @param  security:    1 for ssl, 0 for none
* @param  clientid:      MQTT client id
* @param  client_user:   MQTT client user
* @param  client_pass:   MQTT client password
* @param  keepAliveTime: MQTT keep alive timer, in second
* @param  cleanSession:  On connection, a client sets the "clean session" flag, which is sometimes also known as the "clean start" flag.
*                        If clean session is set to false, then the connection is treated as durable. This means that when the client
*                        disconnects, any subscriptions it has will remain and any subsequent QoS 1 or 2 messages will be stored until
*                        it connects again in the future. If clean session is true, then all subscriptions will be removed for the client
*                        when it disconnects.
* @retval None
*/
void ICACHE_FLASH_ATTR
MQTT_Init(MQTT_Client* client, char* host, uint32 port, uint8_t security, uint8_t sendTimeout,
    char* client_id, char* client_user, char* client_pass,
    uint8_t keepAliveTime) {
  DBG_MQTT("MQTT_Init, host=%s\n", host);

  os_memset(client, 0, sizeof(MQTT_Client));

  client->host = (char*)os_zalloc(os_strlen(host) + 1);
  os_strcpy(client->host, host);

  client->port = port;
  client->security = !!security;

  // timeouts with sanity checks
  client->sendTimeout = sendTimeout == 0 ? 1 : sendTimeout;
  client->reconTimeout = 1; // reset reconnect back-off

  os_memset(&client->connect_info, 0, sizeof(mqtt_connect_info_t));

  client->connect_info.client_id = (char*)os_zalloc(os_strlen(client_id) + 1);
  os_strcpy(client->connect_info.client_id, client_id);

  client->connect_info.username = (char*)os_zalloc(os_strlen(client_user) + 1);
  os_strcpy(client->connect_info.username, client_user);

  client->connect_info.password = (char*)os_zalloc(os_strlen(client_pass) + 1);
  os_strcpy(client->connect_info.password, client_pass);

  client->connect_info.keepalive = keepAliveTime;
  client->connect_info.clean_session = 1;

  client->in_buffer = (uint8_t *)os_zalloc(MQTT_MAX_RCV_MESSAGE);
  client->in_buffer_size = MQTT_MAX_RCV_MESSAGE;

  uint8_t *out_buffer = (uint8_t *)os_zalloc(MQTT_MAX_SHORT_MESSAGE);
  mqtt_msg_init(&client->mqtt_connection, out_buffer, MQTT_MAX_SHORT_MESSAGE);
}

/**
 * @brief  MQTT Set Last Will Topic, must be called before MQTT_Connect
 */
void ICACHE_FLASH_ATTR
MQTT_InitLWT(MQTT_Client* client, char* will_topic, char* will_msg,
    uint8_t will_qos, uint8_t will_retain) {

  client->connect_info.will_topic = (char*)os_zalloc(os_strlen(will_topic) + 1);
  os_strcpy((char*)client->connect_info.will_topic, will_topic);

  client->connect_info.will_message = (char*)os_zalloc(os_strlen(will_msg) + 1);
  os_strcpy((char*)client->connect_info.will_message, will_msg);

  client->connect_info.will_qos = will_qos;
  client->connect_info.will_retain = will_retain;

  // TODO: if we're connected we should disconnect and reconnect to establish the new LWT
}

/**
* @brief  Begin connect to MQTT broker
* @param  client: MQTT_Client reference
* @retval None
*/
void ICACHE_FLASH_ATTR
MQTT_Connect(MQTT_Client* client) {
  //MQTT_Disconnect(client);
  client->pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
  client->pCon->type = ESPCONN_TCP;
  client->pCon->state = ESPCONN_NONE;
  client->pCon->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
  client->pCon->proto.tcp->local_port = espconn_port();
  client->pCon->proto.tcp->remote_port = client->port;
  client->pCon->reverse = client;
  espconn_regist_connectcb(client->pCon, mqtt_tcpclient_connect_cb);
  espconn_regist_reconcb(client->pCon, mqtt_tcpclient_recon_cb);

  // start timer function to tick every second
  os_timer_disarm(&client->mqttTimer);
  os_timer_setfn(&client->mqttTimer, (os_timer_func_t *)mqtt_timer, client);
  os_timer_arm(&client->mqttTimer, 1000, 1);

  // initiate the TCP connection or DNS lookup
  os_printf("MQTT: Connect to %s:%d %p (client=%p)\n",
      client->host, client->port, client->pCon, client);
  if (UTILS_StrToIP((const char *)client->host,
        (void*)&client->pCon->proto.tcp->remote_ip)) {
    uint8_t err;
    if (client->security)
      err = espconn_secure_connect(client->pCon);
    else
      err = espconn_connect(client->pCon);
    if (err != 0) {
      os_printf("MQTT ERROR: Failed to connect\n");
      os_free(client->pCon->proto.tcp);
      os_free(client->pCon);
      client->pCon = NULL;
      return;
    }
  } else {
    espconn_gethostbyname(client->pCon, (const char *)client->host, &client->ip,
        mqtt_dns_found);
  }

  client->connState = TCP_CONNECTING;
  client->timeoutTick = 20; // generous timeout to allow for DNS, etc
  client->sending = FALSE;
}

static void ICACHE_FLASH_ATTR
mqtt_doAbort(MQTT_Client* client) {
  os_printf("MQTT: Disconnecting from %s:%d (%p)\n", client->host, client->port, client->pCon);
  client->pCon->reverse = NULL; // ensure we jettison this pCon...
  if (client->security)
    espconn_secure_disconnect(client->pCon);
  else
    espconn_disconnect(client->pCon);

  if (client->disconnectedCb) client->disconnectedCb(client);
  if (client->cmdDisconnectedCb) client->cmdDisconnectedCb(client);

  if (client->sending_buffer != NULL) {
    os_free(client->sending_buffer);
    client->sending_buffer = NULL;
  }
  client->pCon = NULL;         // it will be freed in disconnect callback
  client->connState = TCP_RECONNECT_REQ;
  client->timeoutTick = client->reconTimeout;     // reconnect in a few seconds
  if (client->reconTimeout < 128) client->reconTimeout <<= 1;
}

void ICACHE_FLASH_ATTR
MQTT_Reconnect(MQTT_Client* client) {
  DBG_MQTT("MQTT: Reconnect requested\n");
  if (client->connState == MQTT_DISCONNECTED)
    MQTT_Connect(client);
  else if (client->connState == MQTT_CONNECTED)
    mqtt_doAbort(client);
  // in other cases we're already in the reconnecting process
}

void ICACHE_FLASH_ATTR
MQTT_Disconnect(MQTT_Client* client) {
  DBG_MQTT("MQTT: Disconnect requested\n");
  os_timer_disarm(&client->mqttTimer);
  if (client->connState == MQTT_DISCONNECTED) return;
  if (client->connState == TCP_RECONNECT_REQ) {
    client->connState = MQTT_DISCONNECTED;
    return;
  }
  mqtt_doAbort(client);
  //void *out_buffer = client->mqtt_connection.buffer;
  //if (out_buffer != NULL) os_free(out_buffer);
  client->connState = MQTT_DISCONNECTED; // ensure we don't automatically reconnect
}

void ICACHE_FLASH_ATTR
MQTT_Free(MQTT_Client* client) {
  DBG_MQTT("MQTT: Free requested\n");
  MQTT_Disconnect(client);

  if (client->host) os_free(client->host);
  client->host = NULL;

  if (client->connect_info.client_id) os_free(client->connect_info.client_id);
  if (client->connect_info.username) os_free(client->connect_info.username);
  if (client->connect_info.password) os_free(client->connect_info.password);
  os_memset(&client->connect_info, 0, sizeof(mqtt_connect_info_t));

  if (client->in_buffer) os_free(client->in_buffer);
  client->in_buffer = NULL;

  if (client->mqtt_connection.buffer) os_free(client->mqtt_connection.buffer);
  os_memset(&client->mqtt_connection, 0, sizeof(client->mqtt_connection));
}

void ICACHE_FLASH_ATTR
MQTT_OnConnected(MQTT_Client* client, MqttCallback connectedCb) {
  client->connectedCb = connectedCb;
}

void ICACHE_FLASH_ATTR
MQTT_OnDisconnected(MQTT_Client* client, MqttCallback disconnectedCb) {
  client->disconnectedCb = disconnectedCb;
}

void ICACHE_FLASH_ATTR
MQTT_OnData(MQTT_Client* client, MqttDataCallback dataCb) {
  client->dataCb = dataCb;
}

void ICACHE_FLASH_ATTR
MQTT_OnPublished(MQTT_Client* client, MqttCallback publishedCb) {
  client->publishedCb = publishedCb;
}
