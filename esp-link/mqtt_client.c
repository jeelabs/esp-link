#include <esp8266.h>
#include "cgiwifi.h"
#include "config.h"
#include "mqtt.h"

MQTT_Client mqttClient;

static ETSTimer mqttTimer;

static int once = 0;
static void ICACHE_FLASH_ATTR
mqttTimerCb(void *arg)
{
  if (once++ > 0) return;
  if (flashConfig.mqtt_enable)
    MQTT_Connect(&mqttClient);
  //MQTT_Subscribe(&mqttClient, "system/time", 0);
}

void ICACHE_FLASH_ATTR
wifiStateChangeCb(uint8_t status)
{
  if (status == wifiGotIP) {
    os_timer_disarm(&mqttTimer);
    os_timer_setfn(&mqttTimer, mqttTimerCb, NULL);
    os_timer_arm(&mqttTimer, 200, 0);
  }
}


// initialize the custom stuff that goes beyond esp-link
void ICACHE_FLASH_ATTR
mqtt_client_init()
{
  MQTT_Init(&mqttClient, flashConfig.mqtt_host, flashConfig.mqtt_port, 0,
      flashConfig.mqtt_timeout, flashConfig.mqtt_clientid,
      flashConfig.mqtt_username, flashConfig.mqtt_password,
      flashConfig.mqtt_keepalive);
  wifiAddStateChangeCb(wifiStateChangeCb);
}
