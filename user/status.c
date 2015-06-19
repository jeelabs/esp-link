// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt

#include <esp8266.h>
#include "config.h"
#include "serled.h"
#include "cgiwifi.h"

static ETSTimer ledTimer;

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
static uint8_t wifiState = 0;

static void ICACHE_FLASH_ATTR ledTimerCb(void *v) {
	int time = 1000;

	if (wifiState == wifiGotIP) {
		// connected, all is good, solid light
		ledState = 1-ledState;
		time = ledState ? 2900 : 100;
	} else if (wifiState == wifiIsConnected) {
		// waiting for DHCP, go on/off every second
		ledState = 1 - ledState;
		time = 1000;
	} else {
		// idle
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

// change the wifi state
void ICACHE_FLASH_ATTR statusWifiUpdate(uint8_t state) {
	wifiState = state;
	// schedule an update (don't want to run into concurrency issues)
	os_timer_disarm(&ledTimer);
	os_timer_setfn(&ledTimer, ledTimerCb, NULL);
	os_timer_arm(&ledTimer, 500, 0);
}

void ICACHE_FLASH_ATTR statusInit(void) {
	if (flashConfig.conn_led_pin >= 0) {
		makeGpio(flashConfig.conn_led_pin);
		setLed(1);
	}
	os_printf("CONN led=%d\n", flashConfig.conn_led_pin);

	os_timer_disarm(&ledTimer);
	os_timer_setfn(&ledTimer, ledTimerCb, NULL);
	os_timer_arm(&ledTimer, 2000, 0);
}


