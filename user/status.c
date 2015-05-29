#include <esp8266.h>
#include "cgiwifi.h"

#define LEDGPIO 0

static ETSTimer ledTimer;

static void ICACHE_FLASH_ATTR setLed(int on) {
	// LED is active-low
	if (on) {
		gpio_output_set(0, (1<<LEDGPIO), (1<<LEDGPIO), 0);
	} else {
		gpio_output_set((1<<LEDGPIO), 0, (1<<LEDGPIO), 0);
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

void ICACHE_FLASH_ATTR statusInit() {
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
	gpio_output_set(0, 0, (1<<LEDGPIO), 0);
	setLed(1);

	os_timer_disarm(&ledTimer);
	os_timer_setfn(&ledTimer, ledTimerCb, NULL);
	os_timer_arm(&ledTimer, 2000, 0);
}


