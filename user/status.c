#include <esp8266.h>

#define LEDGPIO 0

static ETSTimer ledTimer;

void ICACHE_FLASH_ATTR setLed(int on) {

	if (on) {
		gpio_output_set((1<<LEDGPIO), 0, (1<<LEDGPIO), 0);
	} else {
		gpio_output_set(0, (1<<LEDGPIO), (1<<LEDGPIO), 0);
	}
}

static uint8_t ledState = 0;

static void ICACHE_FLASH_ATTR ledTimerCb(void *arg) {
	int m = wifi_get_opmode();
	int c = m == 2 ? 0 : wifi_station_get_connect_status();
	if (c != 5) os_printf("Status: mode=%d conn=%d\n", m, c);
	int time = 1000;


	if (c == STATION_GOT_IP) {
		// connected, all is good, solid light
		ledState = 1-ledState;
		time = ledState ? 2900 : 100;
	} else if (c == STATION_CONNECTING) {
		// connecting, go on/off every second
		ledState = 1 - ledState;
		time = 1000;
	} else if (c > STATION_CONNECTING) {
		// some failure, rapid blinking
		ledState = 1-ledState;
		time = 100;
	} else {
		// idle
		switch (m) {
		case 1: // STA
			ledState = 0;
			break;
		case 2: // AP
			ledState = 1-ledState;
			time = ledState ? 100 : 1900;
			break;
		case 3: // STA+AP
			ledState = 1-ledState;
			time = ledState ? 100 : 900;
			break;
		}
	}

	setLed(1-ledState); // low=on
	os_timer_arm(&ledTimer, time, 0);
}

void statusInit() {
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
	gpio_output_set(0, 0, (1<<LEDGPIO), 0);
	os_timer_disarm(&ledTimer);
	os_timer_setfn(&ledTimer, ledTimerCb, NULL);
	os_timer_arm(&ledTimer, 500, 0);
}


