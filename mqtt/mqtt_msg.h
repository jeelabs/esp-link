/*
* Copyright (c) 2014, Stephen Robinson
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* 3. Neither the name of the copyright holder nor the names of its
* contributors may be used to endorse or promote products derived
* from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
*/

#ifndef MQTT_MSG_H
#define MQTT_MSG_H

#define PROTOCOL_NAMEv311

enum mqtt_message_type {
  MQTT_MSG_TYPE_CONNECT = 1,
  MQTT_MSG_TYPE_CONNACK = 2,
  MQTT_MSG_TYPE_PUBLISH = 3,
  MQTT_MSG_TYPE_PUBACK = 4,
  MQTT_MSG_TYPE_PUBREC = 5,
  MQTT_MSG_TYPE_PUBREL = 6,
  MQTT_MSG_TYPE_PUBCOMP = 7,
  MQTT_MSG_TYPE_SUBSCRIBE = 8,
  MQTT_MSG_TYPE_SUBACK = 9,
  MQTT_MSG_TYPE_UNSUBSCRIBE = 10,
  MQTT_MSG_TYPE_UNSUBACK = 11,
  MQTT_MSG_TYPE_PINGREQ = 12,
  MQTT_MSG_TYPE_PINGRESP = 13,
  MQTT_MSG_TYPE_DISCONNECT = 14
};

// Descriptor for a serialized MQTT message, this is returned by functions that compose a message
// (It's really an MQTT packet in v3.1.1 terminology)
typedef struct mqtt_message {
  uint8_t* data;
  uint16_t length;
} mqtt_message_t;

// Descriptor for a connection with message assembly storage
typedef struct mqtt_connection {
  mqtt_message_t message; // resulting message
  uint16_t message_id;	  // id of assembled message and memo to calculate next message id
  uint8_t* buffer;	  // buffer for assembling messages
  uint16_t buffer_length; // buffer length
} mqtt_connection_t;

// Descriptor for a connect request
typedef struct mqtt_connect_info {
  char* client_id;
  char* username;
  char* password;
  char* will_topic;
  char* will_message;
  uint8_t keepalive;
  uint8_t will_qos;
  uint8_t will_retain;
  uint8_t clean_session;
} mqtt_connect_info_t;

static inline int ICACHE_FLASH_ATTR mqtt_get_type(const uint8_t* buffer) {
  return (buffer[0] & 0xf0) >> 4;
}

static inline int ICACHE_FLASH_ATTR mqtt_get_dup(const uint8_t* buffer) {
  return (buffer[0] & 0x08) >> 3;
}

static inline int ICACHE_FLASH_ATTR mqtt_get_qos(const uint8_t* buffer) {
  return (buffer[0] & 0x06) >> 1;
}

static inline int ICACHE_FLASH_ATTR mqtt_get_retain(const uint8_t* buffer) {
  return (buffer[0] & 0x01);
}

// Init a connection descriptor
void mqtt_msg_init(mqtt_connection_t* connection, uint8_t* buffer, uint16_t buffer_length);

// Returns the total length of a message including MQTT fixed header
int mqtt_get_total_length(const uint8_t* buffer, uint16_t length);

// Return pointer to topic, length in in/out param: in=length of buffer, out=length of topic
const char* mqtt_get_publish_topic(const uint8_t* buffer, uint16_t* length);

// Return pointer to data, length in in/out param: in=length of buffer, out=length of data
const char* mqtt_get_publish_data(const uint8_t* buffer, uint16_t* length);

// Return message id
uint16_t mqtt_get_id(const uint8_t* buffer, uint16_t length);

// The following functions construct an outgoing message
mqtt_message_t* mqtt_msg_connect(mqtt_connection_t* connection, mqtt_connect_info_t* info);
mqtt_message_t* mqtt_msg_publish(mqtt_connection_t* connection, const char* topic, const char* data, int data_length, int qos, int retain, uint16_t* message_id);
mqtt_message_t* mqtt_msg_puback(mqtt_connection_t* connection, uint16_t message_id);
mqtt_message_t* mqtt_msg_pubrec(mqtt_connection_t* connection, uint16_t message_id);
mqtt_message_t* mqtt_msg_pubrel(mqtt_connection_t* connection, uint16_t message_id);
mqtt_message_t* mqtt_msg_pubcomp(mqtt_connection_t* connection, uint16_t message_id);
mqtt_message_t* mqtt_msg_subscribe(mqtt_connection_t* connection, const char* topic, int qos, uint16_t* message_id);
mqtt_message_t* mqtt_msg_unsubscribe(mqtt_connection_t* connection, const char* topic, uint16_t* message_id);
mqtt_message_t* mqtt_msg_pingreq(mqtt_connection_t* connection);
mqtt_message_t* mqtt_msg_pingresp(mqtt_connection_t* connection);
mqtt_message_t* mqtt_msg_disconnect(mqtt_connection_t* connection);

#endif // MQTT_MSG_H

