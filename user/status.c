// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt

#include <esp8266.h>
#include "config.h"
#include "serled.h"
#include "cgiwifi.h"

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

static uint8_t rssiSendState = 0;
static sint8 rssiLast = 0; //last RSSI value

static esp_tcp rssiTcp;
static struct espconn rssiConn;

#define GS_API_KEY "2eb868a8-224f-3faa-939d-c79bd605912a"
#define GS_COMP_ID "esp-link"
#define GS_STREAM  "rssi"

// Connected callback
static void ICACHE_FLASH_ATTR rssiConnectCb(void *arg) {
	struct espconn *conn = (struct espconn *)arg;
	os_printf("RSSI connect CB (%p %p)\n", arg, conn->reverse);

	char buf[2000];

	// http header
	int hdrLen = os_sprintf(buf,
			"PUT /api/feed?api_key=%s HTTP/1.0\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: XXXXX\r\n\r\n",
			GS_API_KEY);

	// http body
	int dataLen = os_sprintf(buf+hdrLen,
			"[{\"compId\":\"%s\", \"streamId\":\"%s\", \"data\":%d}]",
			GS_COMP_ID, GS_STREAM, rssiLast);

	// hackish way to fill in the content-length
	os_sprintf(buf+hdrLen-9, "%5d", dataLen);
	buf[hdrLen-4] = '\r';

	// send it off
	if (espconn_sent(conn, (uint8*)buf, hdrLen+dataLen) == ESPCONN_OK) {
		os_printf("RSSI sent rssi=%d\n", rssiLast);
		os_printf("RSSI sent <<%s>>\n", buf);
	}
}

// Sent callback
static void ICACHE_FLASH_ATTR rssiSentCb(void *arg) {
	struct espconn *conn = (struct espconn *)arg;
	os_printf("RSSI sent CB (%p %p)\n", arg, conn->reverse);
}

// Recv callback
static void ICACHE_FLASH_ATTR rssiRecvCb(void *arg, char *data, uint16_t len) {
	struct espconn *conn = (struct espconn *)arg;
	os_printf("RSSI recv CB (%p %p)\n", arg, conn->reverse);
	data[len] = 0; // hack!!!
	os_printf("GOT %d: <<%s>>\n", len, data);

	espconn_disconnect(conn);
}

// Disconnect callback
static void ICACHE_FLASH_ATTR rssiDisconCb(void *arg) {
	struct espconn *conn = (struct espconn *)arg;
	os_printf("RSSI disconnect CB (%p %p)\n", arg, conn->reverse);
	rssiSendState = 0;
}

// Connection reset callback
static void ICACHE_FLASH_ATTR rssiResetCb(void *arg, sint8 err) {
	struct espconn *conn = (struct espconn *)arg;
	os_printf("RSSI reset CB (%p %p) err=%d\n", arg, conn->reverse, err);
	rssiSendState = 0;
}

// Timer callback to send an RSSI update to a monitoring system
static void ICACHE_FLASH_ATTR rssiTimerCb(void *v) {
	sint8 rssi = wifi_station_get_rssi();
	if (rssi >= 0) return; // not connected or other error
	rssiLast = rssi;
	if (rssiSendState > 0) return; // not done with previous rssi report

	rssiConn.type = ESPCONN_TCP;
	rssiConn.state = ESPCONN_NONE;
	rssiConn.proto.tcp = &rssiTcp;
	rssiTcp.remote_port = 80;
	rssiTcp.remote_ip[0] = 173;
  rssiTcp.remote_ip[1] = 236;
  rssiTcp.remote_ip[2] = 12;
  rssiTcp.remote_ip[3] = 163;
  espconn_regist_connectcb(&rssiConn, rssiConnectCb);
	espconn_regist_reconcb(&rssiConn, rssiResetCb);
	espconn_regist_sentcb(&rssiConn, rssiSentCb);
	espconn_regist_recvcb(&rssiConn, rssiRecvCb);
	espconn_regist_disconcb(&rssiConn, rssiDisconCb);
	rssiConn.reverse = (void *)0xdeadf00d;
	os_printf("RSSI connect (%p)\n", &rssiConn);
	if (espconn_connect(&rssiConn) == ESPCONN_OK) {
		rssiSendState++;
	}
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


