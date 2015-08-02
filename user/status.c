// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt

#include <esp8266.h>
#include "config.h"
#include "serled.h"
#include "cgiwifi.h"
#include "tcpclient.h"

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
static uint8_t wifiState = 0;

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

//===== RSSI Status update sent to GroveStreams

#define RSSI_INTERVAL (60*1000)

static ETSTimer rssiTimer;

#define GS_STREAM  "rssi"

// Timer callback to send an RSSI update to a monitoring system
static void ICACHE_FLASH_ATTR rssiTimerCb(void *v) {
	if (!flashConfig.rssi_enable || !flashConfig.tcp_enable || flashConfig.api_key[0]==0)
		return;

	sint8 rssi = wifi_station_get_rssi();
	if (rssi >= 0) return; // not connected or other error

	// compose TCP command
	uint8_t chan = MAX_TCP_CHAN-1;
	tcpClientCommand(chan, 'T', "grovestreams.com:80");

	// compose http header
	char buf[1024];
	int hdrLen = os_sprintf(buf,
			"PUT /api/feed?api_key=%s HTTP/1.0\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: XXXXX\r\n\r\n",
			flashConfig.api_key);

	// http body
	int dataLen = os_sprintf(buf+hdrLen,
			"[{\"compId\":\"%s\", \"streamId\":\"%s\", \"data\":%d}]\r",
			flashConfig.hostname, GS_STREAM, rssi);
	buf[hdrLen+dataLen++] = 0;
	buf[hdrLen+dataLen++] = '\r';

	// hackish way to fill in the content-length
	os_sprintf(buf+hdrLen-9, "%5d", dataLen);
	buf[hdrLen-4] = '\r'; // fix-up the \0 inserted by sprintf (hack!)

	// send the request off and forget about it...
	for (short i=0; i<hdrLen+dataLen; i++) {
		tcpClientSendChar(chan, buf[i]);
	}
	tcpClientSendPush(chan);
}

//===== Init status stuff

void ICACHE_FLASH_ATTR statusInit(void) {
	if (flashConfig.conn_led_pin >= 0) {
		makeGpio(flashConfig.conn_led_pin);
		setLed(1);
	}
	os_printf("CONN led=%d\n", flashConfig.conn_led_pin);

	os_timer_disarm(&ledTimer);
	os_timer_setfn(&ledTimer, ledTimerCb, NULL);
	os_timer_arm(&ledTimer, 2000, 0);

	os_timer_disarm(&rssiTimer);
	os_timer_setfn(&rssiTimer, rssiTimerCb, NULL);
	os_timer_arm(&rssiTimer, RSSI_INTERVAL, 1); // recurring timer
}


