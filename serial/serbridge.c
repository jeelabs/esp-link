#include "espmissingincludes.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"
#include "gpio.h"

#include "uart.h"
#include "serbridge.h"

#if 1
// GPIO for esp-03 module with gpio12->reset, gpio13->isp, gpio2->"ser" LED
#define MCU_RESET 12
#define MCU_ISP   13
#define MCU_LED    2
#else
// GPIO for esp-01 module with gpio0->reset, gpio2->isp
#define MCU_RESET  0
#define MCU_ISP    2
#undef MCU_LED
#endif

static struct espconn serbridgeConn;
static esp_tcp serbridgeTcp;

sint8  ICACHE_FLASH_ATTR espbuffsend(serbridgeConnData *conn, const char *data, uint16 len);

// Connection pool
serbridgeConnData connData[MAX_CONN];
// Transmit buffers for the connection pool
static char txbuffer[MAX_CONN][MAX_TXBUFFER];

// Given a pointer to an espconn struct find the connection that correcponds to it
static serbridgeConnData ICACHE_FLASH_ATTR *serbridgeFindConnData(void *arg) {
	for (int i=0; i<MAX_CONN; i++) {
		if (connData[i].conn == (struct espconn *)arg) {
			return &connData[i];
		}
	}
	//os_printf("FindConnData: Huh? Couldn't find connection for %p\n", arg);
	return NULL; // not found, may be closed already...
}

// Send all data in conn->txbuffer
// returns result from espconn_sent if data in buffer or ESPCONN_OK (0)
// Use only internally from espbuffsend and serbridgeSentCb
static sint8 ICACHE_FLASH_ATTR sendtxbuffer(serbridgeConnData *conn) {
	sint8 result = ESPCONN_OK;
	if (conn->txbufferlen != 0) {
		//os_printf("%d TX %d\n", system_get_time(), conn->txbufferlen);
		conn->readytosend = false;
		result = espconn_sent(conn->conn, (uint8_t*)conn->txbuffer, conn->txbufferlen);
		conn->txbufferlen = 0;
		if (result != ESPCONN_OK) {
			os_printf("sendtxbuffer: espconn_sent error %d on conn %p\n", result, conn);
		}
	}
	return result;
}

// espbuffsend adds data to the send buffer. If the previous send was completed it calls
// sendtxbuffer and espconn_sent.
// Returns ESPCONN_OK (0) for success, -128 if buffer is full or error from  espconn_sent
// Use espbuffsend instead of espconn_sent as it solves the problem that espconn_sent must
// only be called *after* receiving an espconn_sent_callback for the previous packet.
sint8 ICACHE_FLASH_ATTR espbuffsend(serbridgeConnData *conn, const char *data, uint16 len) {
	if (conn->txbufferlen + len > MAX_TXBUFFER) {
		os_printf("espbuffsend: txbuffer full on conn %p\n", conn);
		return -128;
	}
	os_memcpy(conn->txbuffer + conn->txbufferlen, data, len);
	conn->txbufferlen += len;
	if (conn->readytosend) {
		return sendtxbuffer(conn);
	} else {
		//os_printf("%d QU %d\n", system_get_time(), conn->txbufferlen);
	}
	return ESPCONN_OK;
}

//callback after the data are sent
static void ICACHE_FLASH_ATTR serbridgeSentCb(void *arg) {
	serbridgeConnData *conn = serbridgeFindConnData(arg);
	//os_printf("Sent callback on conn %p\n", conn);
	if (conn == NULL) return;
	//os_printf("%d ST\n", system_get_time());
	conn->readytosend = true;
	sendtxbuffer(conn); // send possible new data in txbuffer
}

// Receive callback
static void ICACHE_FLASH_ATTR serbridgeRecvCb(void *arg, char *data, unsigned short len) {
	serbridgeConnData *conn = serbridgeFindConnData(arg);
	//os_printf("Receive callback on conn %p\n", conn);
	if (conn == NULL) return;

	// at the start of a connection we're in cmInit mode and we wait for the first few characters
	// to arrive in order to decide what type of connection this is.. The following if statements
	// do this dispatch. An issue here is that we assume that the first few characters all arrive
	// in the same TCP packet, which is true if the sender is a program, but not necessarily
	// if the sender is a person typing (although in that case the line-oriented TTY input seems
	// to make it work too). If this becomes a problem we need to buffer the first few chars...
	if (conn->conn_mode == cmInit) {

		// If the connection starts with the Arduino or ARM reset sequence we perform a RESET
		if ((len == 2 && strncmp(data, "0 ", 2) == 0) ||
				(len == 2 && strncmp(data, "?\n", 2) == 0) ||
				(len == 3 && strncmp(data, "?\r\n", 3) == 0)) {
			os_printf("MCU Reset=%d ISP=%d\n", MCU_RESET, MCU_ISP);
			// send reset to arduino/ARM
			GPIO_OUTPUT_SET(MCU_RESET, 0);
			os_delay_us(100L);
			GPIO_OUTPUT_SET(MCU_ISP, 0);
			os_delay_us(1000L);
			GPIO_OUTPUT_SET(MCU_RESET, 1);
			os_delay_us(100L);
			GPIO_OUTPUT_SET(MCU_ISP, 1);
			os_delay_us(1000L);
			//uart0_tx_buffer(data, len);
			//conn->skip_chars = 2;
			conn->conn_mode = cmAVR;
			//return;
		} else {
			conn->conn_mode = cmTransparent;
		}
	}

	uart0_tx_buffer(data, len);
}

// Error callback (it's really poorly named, it's not a "connection reconnected" callback,
// it's really a "connection broken, please reconnect" callback)
static void ICACHE_FLASH_ATTR serbridgeReconCb(void *arg, sint8 err) {
	serbridgeConnData *conn=serbridgeFindConnData(arg);
	if (conn == NULL) return;
	// Close the connection
	espconn_disconnect(conn->conn);
	conn->conn = NULL;
}

// Disconnection callback
static void ICACHE_FLASH_ATTR serbridgeDisconCb(void *arg) {
	// Iterate through all the connections and deallocate the ones that are in a state that
	// indicates that they're closed
	for (int i=0; i<MAX_CONN; i++) {
		if (connData[i].conn != NULL &&
		   (connData[i].conn->state == ESPCONN_NONE || connData[i].conn->state == ESPCONN_CLOSE))
		{
			connData[i].conn = NULL;
		}
	}
}

// New connection callback, use one of the connection descriptors, if we have one left.
static void ICACHE_FLASH_ATTR serbridgeConnectCb(void *arg) {
	struct espconn *conn = arg;
	//Find empty conndata in pool
	int i;
	for (i=0; i<MAX_CONN; i++) if (connData[i].conn==NULL) break;
	os_printf("Accept port 23, conn=%p, pool slot %d\n", conn, i);

	if (i==MAX_CONN) {
		os_printf("Aiee, conn pool overflow!\n");
		espconn_disconnect(conn);
		return;
	}

	connData[i].conn=conn;
	connData[i].txbufferlen = 0;
	connData[i].readytosend = true;
	connData[i].skip_chars = 0;
	connData[i].conn_mode = cmInit;

	espconn_regist_recvcb(conn, serbridgeRecvCb);
	espconn_regist_reconcb(conn, serbridgeReconCb);
	espconn_regist_disconcb(conn, serbridgeDisconCb);
	espconn_regist_sentcb(conn, serbridgeSentCb);
}

// callback with a buffer of characters that have arrived on the uart
void ICACHE_FLASH_ATTR
serbridgeUartCb(char *buf, int length) {
		// push the buffer into each open connection
		int s = 0;
		for (int i = 0; i < MAX_CONN; ++i) {
			if (connData[i].conn) {
				s++;
				if (connData[i].skip_chars == 0) {
					espbuffsend(&connData[i], buf, length);
				} else if (connData[i].skip_chars >= length) {
					connData[i].skip_chars -= length;
				} else { // connData[i].skip_chars < length
					espbuffsend(&connData[i], buf+connData[i].skip_chars, length-connData[i].skip_chars);
					connData[i].skip_chars = 0;
				}
			}
		}
}

// Start transparent serial bridge TCP server on specified port (typ. 23)
void ICACHE_FLASH_ATTR serbridgeInit(int port) {
	int i;
	for (i = 0; i < MAX_CONN; i++) {
		connData[i].conn = NULL;
		connData[i].txbuffer = txbuffer[i];
	}
	serbridgeConn.type = ESPCONN_TCP;
	serbridgeConn.state = ESPCONN_NONE;
	serbridgeTcp.local_port = port;
	serbridgeConn.proto.tcp = &serbridgeTcp;

	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U , FUNC_GPIO13);
	GPIO_OUTPUT_SET(MCU_ISP, 1);
	GPIO_OUTPUT_SET(MCU_RESET, 0);
#ifdef MCU_LED
	//GPIO_OUTPUT_SET(MCU_LED, 1);
#endif

	espconn_regist_connectcb(&serbridgeConn, serbridgeConnectCb);
	espconn_accept(&serbridgeConn);
	espconn_regist_time(&serbridgeConn, SER_BRIDGE_TIMEOUT, 0);
}
