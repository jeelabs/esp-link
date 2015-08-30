#include <esp8266.h>
#include <mqtt.h>
#include <cgiwifi.h>
#include <json/jsontree.h>
#include <json/jsonparse.h>
#include "user_json.h"
#include "user_funcs.h"

MQTT_Client mqttClient;

typedef struct {
  uint8_t fallbackStateBits;
  uint8_t stateBits;
  uint8_t init;
  uint8_t fallbackSecondsForBits[8];
} LatchState;

static LatchState latch;

static void ICACHE_FLASH_ATTR
updateLatch() {
  os_printf("ESP: Latch Callback\n");
  cmdCallback* latchCb = CMD_GetCbByName("Latch");
  if (latchCb->callback != -1) {
//    uint16_t crc = CMD_ResponseStart(CMD_SENSOR_EVENTS, (uint32_t)&latchCb->callback, 0, 1);
//    crc = CMD_ResponseBody(crc, (uint8_t*)&latch, sizeof(LatchState));
//    CMD_ResponseEnd(crc);
  }
}

LOCAL int ICACHE_FLASH_ATTR
latchGet(struct jsontree_context *js_ctx) {
  return 0;
}

LOCAL int ICACHE_FLASH_ATTR
latchSet(struct jsontree_context *js_ctx, struct jsonparse_state *parser) {
  int type;
  int ix = -1;
  bool sendLatchUpdate = false;
  while ((type = jsonparse_next(parser)) != 0) {
    if (type == JSON_TYPE_ARRAY) {
      ix = -1;
    }
    else if (type == JSON_TYPE_OBJECT) {
      ix++;
    }
    else if (type == JSON_TYPE_PAIR_NAME) {
      if (jsonparse_strcmp_value(parser, "states") == 0) {        
        char latchStates[9];
        jsonparse_next(parser); jsonparse_next(parser);
        jsonparse_copy_value(parser, latchStates, sizeof(latchStates));
        os_printf("latch states %s\n", latchStates);
        uint8_t states = binToByte(latchStates);
//        if (latch.stateBits != states) {
          latch.stateBits = states;
          sendLatchUpdate = true;
//        }
      }
      else if (jsonparse_strcmp_value(parser, "fallbackstates") == 0) {
        char fallbackStates[9];
        jsonparse_next(parser); jsonparse_next(parser);
        jsonparse_copy_value(parser, fallbackStates, sizeof(fallbackStates));
        os_printf("latch states %s\n", fallbackStates);
        uint8_t fbstates = binToByte(fallbackStates);
//        if (latch.fallbackStateBits != fbstates) {
          latch.fallbackStateBits = fbstates;
          sendLatchUpdate = true;
//        }
      }
    }

    if (sendLatchUpdate) {
      updateLatch();
    }
  }
  return 0;
}

static struct jsontree_callback latchCallback = JSONTREE_CALLBACK(latchGet, latchSet);
static char* latchQueueName;

JSONTREE_OBJECT(latchJsonObj,
  JSONTREE_PAIR("states", &latchCallback),
  JSONTREE_PAIR("fallbackstates", &latchCallback));

void ICACHE_FLASH_ATTR 
mqttConnectedCb(uint32_t *args) {
  MQTT_Client* client = (MQTT_Client*)args;
  MQTT_Publish(client, "announce/all", "Hello World!", 0, 0);
  
  char* latchQueue = "/latch";
  char *buff = (char*)os_zalloc(strlen(system_get_chip_id_str()) + strlen(latchQueue) + 1);
  os_strcpy(buff, system_get_chip_id_str());
  os_strcat(buff, latchQueue);
  latchQueueName = buff;
  MQTT_Subscribe(client, latchQueueName, 0);
}

void ICACHE_FLASH_ATTR 
mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len) {
  char *topicBuf = (char*)os_zalloc(topic_len + 1);
  char *dataBuf = (char*)os_zalloc(data_len + 1);

//  MQTT_Client* client = (MQTT_Client*)args;

  os_memcpy(topicBuf, topic, topic_len);
  topicBuf[topic_len] = 0;

  os_memcpy(dataBuf, data, data_len);
  dataBuf[data_len] = 0;

  os_printf("Receive topic: %s\n  Data: %s\n", topicBuf, dataBuf);

  if (!strcoll(topicBuf, latchQueueName)) {
    struct jsontree_context js;
    jsontree_setup(&js, (struct jsontree_value *)&latchJsonObj, json_putchar);
    json_parse(&js, dataBuf);
  }

  os_free(topicBuf);
  os_free(dataBuf);
}

void ICACHE_FLASH_ATTR 
wifiStateChangeCb(uint8_t status)
{
  if (status == wifiGotIP  && mqttClient.connState != TCP_CONNECTING){
    MQTT_Connect(&mqttClient);
  }
  else if (status == wifiIsDisconnected && mqttClient.connState == TCP_CONNECTING){    
    MQTT_Disconnect(&mqttClient);
  }
}

void ICACHE_FLASH_ATTR
mqttDisconnectedCb(uint32_t *args) {
  //  MQTT_Client* client = (MQTT_Client*)args;
  os_printf("MQTT Disconnected\n");
}

void ICACHE_FLASH_ATTR
mqttTcpDisconnectedCb(uint32_t *args) {
  //  MQTT_Client* client = (MQTT_Client*)args;
  os_printf("MQTT TCP Disconnected\n");
}

void ICACHE_FLASH_ATTR
mqttPublishedCb(uint32_t *args) {
  //  MQTT_Client* client = (MQTT_Client*)args;
  os_printf("MQTT Published\n");
}

void init() {
  wifiAddStateChangeCb(wifiStateChangeCb);
  MQTT_InitConnection(&mqttClient, MQTT_HOST, MQTT_PORT, MQTT_SECURITY);
  MQTT_InitClient(&mqttClient, MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS, MQTT_KEEPALIVE, MQTT_CLSESSION);
  MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
  MQTT_OnConnected(&mqttClient, mqttConnectedCb);
  MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
  MQTT_OnDisconnected(&mqttClient, mqttTcpDisconnectedCb);
  MQTT_OnPublished(&mqttClient, mqttPublishedCb);
  MQTT_OnData(&mqttClient, mqttDataCb);
}