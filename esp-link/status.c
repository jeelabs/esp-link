// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt

#include <esp8266.h>
#include "config.h"
#include "serled.h"
#include "cgiwifi.h"

#ifdef MQTT
#include "mqtt.h"
#include "mqtt_client.h"
extern MQTT_Client mqttClient;

//===== MQTT Status update

// Every minute...
#define MQTT_STATUS_INTERVAL (60*1000)

static ETSTimer mqttStatusTimer;

int ICACHE_FLASH_ATTR
mqttStatusMsg(char *buf) {
  sint8 rssi = wifi_station_get_rssi();
  if (rssi > 0) rssi = 0; // not connected or other error
  //os_printf("timer rssi=%d\n", rssi);

  // compose MQTT message
  return os_sprintf(buf,
    "{\"rssi\":%d, \"heap_free\":%ld}",
    rssi, (unsigned long)system_get_free_heap_size());
}

// Timer callback to send an RSSI update to a monitoring system
static void ICACHE_FLASH_ATTR mqttStatusCb(void *v) {
  if (!flashConfig.mqtt_status_enable || os_strlen(flashConfig.mqtt_status_topic) == 0 ||
    mqttClient.connState != MQTT_CONNECTED)
    return;

  char buf[128];
  mqttStatusMsg(buf);
  MQTT_Publish(&mqttClient, flashConfig.mqtt_status_topic, buf, os_strlen(buf), 1, 0);
}



#endif // MQTT

//===== "CONN" LED status indication

static ETSTimer ledTimer;

// Set the LED on or off, respecting the defined polarity
static void ICACHE_FLASH_ATTR setLed(int on) {
  int8_t pin = flashConfig.conn_led_pin;
  if (pin < 0) return; // disabled
  // LED is active-low
  if (on) {
    gpio_output_set(0, (1<<pin), (1<<pin), 0);
  } else {
    gpio_output_set((1<<pin), 0, (1<<pin), 0);
  }
}

static uint8_t ledState = 0;

// Timer callback to update the LED
static void ICACHE_FLASH_ATTR ledTimerCb(void *v) {
  int time = 1000;

  if (wifiState == wifiGotIP) {
    // connected, all is good, solid light with a short dark blip every 3 seconds
    ledState = 1-ledState;
    time = ledState ? 2900 : 100;
  } else if (wifiState == wifiIsConnected) {
    // waiting for DHCP, go on/off every second
    ledState = 1 - ledState;
    time = 1000;
  } else {
    // not connected
    switch (wifi_get_opmode()) {
    case 1: // STA
      ledState = 0;
      break;
    case 2: // AP
      ledState = 1-ledState;
      time = ledState ? 50 : 1950;
      break;
    case 3: // STA+AP
      ledState = 1-ledState;
      time = ledState ? 50 : 950;
      break;
    }
  }

  setLed(ledState);
  os_timer_arm(&ledTimer, time, 0);
}

// change the wifi state indication
void ICACHE_FLASH_ATTR statusWifiUpdate(uint8_t state) {
  wifiState = state;
  // schedule an update (don't want to run into concurrency issues)
  os_timer_disarm(&ledTimer);
  os_timer_setfn(&ledTimer, ledTimerCb, NULL);
  os_timer_arm(&ledTimer, 500, 0);
}

//===== Init status stuff

void ICACHE_FLASH_ATTR statusInit(void) {
  if (flashConfig.conn_led_pin >= 0) {
    makeGpio(flashConfig.conn_led_pin);
    setLed(1);
  }
#ifdef STATUS_DBG
  os_printf("CONN led=%d\n", flashConfig.conn_led_pin);
#endif

  os_timer_disarm(&ledTimer);
  os_timer_setfn(&ledTimer, ledTimerCb, NULL);
  os_timer_arm(&ledTimer, 2000, 0);

#ifdef MQTT
  os_timer_disarm(&mqttStatusTimer);
  os_timer_setfn(&mqttStatusTimer, mqttStatusCb, NULL);
  os_timer_arm(&mqttStatusTimer, MQTT_STATUS_INTERVAL, 1); // recurring timer
#endif // MQTT
}


