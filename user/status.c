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
	//os_printf("Timer Hello\n");
	setLed((ledState++)&1);
}

void statusInit() {
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
	gpio_output_set(0, 0, (1<<LEDGPIO), 0);
	os_timer_disarm(&ledTimer);
	os_timer_setfn(&ledTimer, ledTimerCb, NULL);
	os_timer_arm(&ledTimer, 500, 1);
}


