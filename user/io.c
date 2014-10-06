#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"
#include "gpio.h"

void ICACHE_FLASH_ATTR ioLed(int ena) {
	if (ena) {
		gpio_output_set(BIT2, 0, BIT2, 0);
	} else {
		gpio_output_set(0, BIT2, BIT2, 0);
	}
}

void ioInit() {
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
}