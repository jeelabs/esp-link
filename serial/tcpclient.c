// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt
//
// TCP client library allowing uControllers attached to the serial port to send commands
// to open/close TCP connections and send/recv data.
// The serial protocol is described in https://gist.github.com/tve/a46c44bf1f6b42bc572e

#include <esp8266.h>
#include "config.h"
#include "uart.h"
#include "serled.h"
#include "tcpclient.h"

// max number of channels the client can open
#define MAX_CHAN MAX_TCP_CHAN
// size of tx buffer
#define MAX_TXBUF 1024

enum TcpState {
	TCP_idle,    // unused connection
	TCP_dns,     // doing gethostbyname
	TCP_conn,    // connecting to remote server
	TCP_data,    // connected
};

// Connections
typedef struct {
	struct espconn *conn; // esp connection structure
	esp_tcp *tcp;         // esp TCP parameters
	char *txBuf;          // buffer to accumulate into
	char *txBufSent;      // buffer held by espconn
	uint8_t txBufLen;     // number of chars in txbuf
	enum TcpState state;
} TcpConn;

static TcpConn tcpConn[MAX_CHAN];

// forward declarations
static void tcpConnFree(TcpConn* tci);
static TcpConn* tcpConnAlloc(uint8_t chan);
static void tcpDoSend(TcpConn *tci);
static void tcpConnectCb(void *arg);
static void tcpDisconCb(void *arg);
static void tcpResetCb(void *arg, sint8 err);
static void tcpSentCb(void *arg);
static void tcpRecvCb(void *arg, char *data, uint16_t len);

//===== allocate / free connections

// Allocate a new connection dynamically and return it. Returns NULL if buf alloc failed
static TcpConn* ICACHE_FLASH_ATTR
tcpConnAlloc(uint8_t chan) {
	TcpConn *tci = tcpConn+chan;
	if (tci->state != TCP_idle && tci->conn != NULL) return tci;

	// malloc and return espconn struct
	tci->conn = os_malloc(sizeof(struct espconn));
	if (tci->conn == NULL) goto fail;
	memset(tci->conn, 0, sizeof(struct espconn));
	// malloc esp_tcp struct
	tci->tcp = os_malloc(sizeof(esp_tcp));
	if (tci->tcp == NULL) goto fail;
	memset(tci->tcp, 0, sizeof(esp_tcp));

	// common init
	tci->state = TCP_dns;
	tci->conn->type = ESPCONN_TCP;
	tci->conn->state = ESPCONN_NONE;
	tci->conn->proto.tcp = tci->tcp;
	tci->tcp->remote_port = 80;
	espconn_regist_connectcb(tci->conn, tcpConnectCb);
	espconn_regist_reconcb(tci->conn, tcpResetCb);
	espconn_regist_sentcb(tci->conn, tcpSentCb);
	espconn_regist_recvcb(tci->conn, tcpRecvCb);
	espconn_regist_disconcb(tci->conn, tcpDisconCb);
	tci->conn->reverse = tci;

	return tci;

fail:
	tcpConnFree(tci);
	return NULL;
}

// Free a connection dynamically.
static void ICACHE_FLASH_ATTR
tcpConnFree(TcpConn* tci) {
	if (tci->conn != NULL) os_free(tci->conn);
	if (tci->tcp != NULL) os_free(tci->tcp);
	if (tci->txBuf != NULL) os_free(tci->txBuf);
	if (tci->txBufSent != NULL) os_free(tci->txBufSent);
	memset(tci, 0, sizeof(TcpConn));
}

//===== DNS

// DNS name resolution callback
static void ICACHE_FLASH_ATTR
tcpClientHostnameCb(const char *name, ip_addr_t *ipaddr, void *arg) {
	struct espconn *conn = arg;
	TcpConn *tci = conn->reverse;
	os_printf("TCP dns CB (%p %p)\n", arg, tci);
	if (ipaddr == NULL) {
		os_printf("TCP %s not found\n", name);
	} else {
		os_printf("TCP %s -> %d.%d.%d.%d\n", name, IP2STR(ipaddr));
		tci->tcp->remote_ip[0] = ip4_addr1(ipaddr);
		tci->tcp->remote_ip[1] = ip4_addr2(ipaddr);
		tci->tcp->remote_ip[2] = ip4_addr3(ipaddr);
		tci->tcp->remote_ip[3] = ip4_addr4(ipaddr);
		os_printf("TCP connect %d.%d.%d.%d (%p)\n", IP2STR(tci->tcp->remote_ip), tci);
		if (espconn_connect(tci->conn) == ESPCONN_OK) {
			tci->state = TCP_conn;
			return;
		}
		os_printf("TCP connect failure\n");
	}
	// oops
	tcpConnFree(tci);
}

//===== Connect / disconnect

// Connected callback
static void ICACHE_FLASH_ATTR
tcpConnectCb(void *arg) {
	struct espconn *conn = arg;
	TcpConn *tci = conn->reverse;
	os_printf("TCP connect CB (%p %p)\n", arg, tci);
	tci->state = TCP_data;
	// send any buffered data
	if (tci->txBuf != NULL && tci->txBufLen > 0) tcpDoSend(tci);
	// reply to serial
	char buf[6];
	short l = os_sprintf(buf, "\n~@%dC\n", tci-tcpConn);
	uart0_tx_buffer(buf, l);
}

// Disconnect callback
static void ICACHE_FLASH_ATTR tcpDisconCb(void *arg) {
	struct espconn *conn = arg;
	TcpConn *tci = conn->reverse;
	os_printf("TCP disconnect CB (%p %p)\n", arg, tci);
	// notify to serial
	char buf[6];
	short l = os_sprintf(buf, "\n~@%dZ\n", tci-tcpConn);
	uart0_tx_buffer(buf, l);
	// free
	tcpConnFree(tci);
}

// Connection reset callback
static void ICACHE_FLASH_ATTR tcpResetCb(void *arg, sint8 err) {
	struct espconn *conn = arg;
	TcpConn *tci = conn->reverse;
	os_printf("TCP reset CB (%p %p) err=%d\n", arg, tci, err);
	// notify to serial
	char buf[6];
	short l = os_sprintf(buf, "\n~@%dZ\n", tci-tcpConn);
	uart0_tx_buffer(buf, l);
	// free
	tcpConnFree(tci);
}

//===== Sending and receiving

// Send the next buffer (assumes that the connection is in a state that allows it)
static void ICACHE_FLASH_ATTR
tcpDoSend(TcpConn *tci) {
	sint8 err = espconn_sent(tci->conn, (uint8*)tci->txBuf, tci->txBufLen);
	if (err == ESPCONN_OK) {
		// send successful
		os_printf("TCP sent (%p %p)\n", tci->conn, tci);
		tci->txBuf[tci->txBufLen] = 0; os_printf("TCP data: %s\n", tci->txBuf);
		tci->txBufSent = tci->txBuf;
		tci->txBuf = NULL;
		tci->txBufLen = 0;
	} else {
		// send error, leave as-is and try again later...
		os_printf("TCP send err (%p %p) %d\n", tci->conn, tci, err);
	}
}

// Sent callback
static void ICACHE_FLASH_ATTR
tcpSentCb(void *arg) {
	struct espconn *conn = arg;
	TcpConn *tci = conn->reverse;
	os_printf("TCP sent CB (%p %p)\n", arg, tci);
	if (tci->txBufSent != NULL) os_free(tci->txBufSent);
	tci->txBufSent = NULL;

	if (tci->txBuf != NULL && tci->txBufLen == MAX_TXBUF) {
		// next buffer is full, send it now
		tcpDoSend(tci);
	}
}

// Recv callback
static void ICACHE_FLASH_ATTR tcpRecvCb(void *arg, char *data, uint16_t len) {
	struct espconn *conn = arg;
	TcpConn *tci = conn->reverse;
	os_printf("TCP recv CB (%p %p)\n", arg, tci);
	if (tci->state == TCP_data) {
		uint8_t chan;
		for (chan=0; chan<MAX_CHAN && tcpConn+chan!=tci; chan++)
		if (chan >= MAX_CHAN) return; // oops!?
		char buf[6];
		short l = os_sprintf(buf, "\n~%d", chan);
		uart0_tx_buffer(buf, l);
		uart0_tx_buffer(data, len);
		uart0_tx_buffer("\0\n", 2);
	}
	serledFlash(50); // short blink on serial LED
}

void ICACHE_FLASH_ATTR
tcpClientSendChar(uint8_t chan, char c) {
	TcpConn *tci = tcpConn+chan;
	if (tci->state == TCP_idle) return;

	if (tci->txBuf != NULL) {
		// we have a buffer
		if (tci->txBufLen < MAX_TXBUF) {
			// buffer has space, add char and return
			tci->txBuf[tci->txBufLen++] = c;
			return;
		} else if (tci->txBufSent == NULL) {
			// we don't have a send pending, send full buffer off
			if (tci->state == TCP_data) tcpDoSend(tci);
			if (tci->txBuf != NULL) return; // something went wrong
		} else {
			// buffers all backed-up, drop char
			return;
		}
	}
	// we do not have a buffer (either didn't have one or sent it off)
	// allocate one
	tci->txBuf = os_malloc(MAX_TXBUF);
	tci->txBufLen = 0;
	if (tci->txBuf != NULL) {
		tci->txBuf[tci->txBufLen++] = c;
	}
}

void ICACHE_FLASH_ATTR
tcpClientSendPush(uint8_t chan) {
	TcpConn *tci = tcpConn+chan;
	if (tci->state != TCP_data) return; // no active connection on this channel
	if (tci->txBuf == NULL || tci->txBufLen == 0) return; // no chars accumulated to send
	if (tci->txBufSent != NULL) return; // already got a send in progress
	tcpDoSend(tci);
}

//===== Command parsing

// Perform a TCP command: parse the command and do the right thing.
// Returns true on success.
bool ICACHE_FLASH_ATTR
tcpClientCommand(uint8_t chan, char cmd, char *cmdBuf) {
	TcpConn *tci;
	char *hostname;
	char *port;

	switch (cmd) {
	//== TCP Connect command
	case 'T':
		hostname = cmdBuf;
		port = hostname;
		while (*port != 0 && *port != ':') port++;
		if (*port != ':') break;
		*port = 0;
		port++;
		int portInt = atoi(port);
		if (portInt < 1 || portInt > 65535) break;

		// allocate a connection
		tci = tcpConnAlloc(chan);
		if (tci == NULL) break;
		tci->state = TCP_dns;
		tci->tcp->remote_port = portInt;

		// start the DNS resolution
		os_printf("TCP %p resolving %s for chan %d (conn=%p)\n", tci, hostname, chan ,tci->conn);
		ip_addr_t ip;
		err_t err = espconn_gethostbyname(tci->conn, hostname, &ip, tcpClientHostnameCb);
		if (err == ESPCONN_OK) {
			// dns cache hit, got the IP address, fake the callback (sigh)
			os_printf("TCP DNS hit\n");
			tcpClientHostnameCb(hostname, &ip, tci->conn);
		} else if (err != ESPCONN_INPROGRESS) {
			tcpConnFree(tci);
			break;
		}

		return true;

	//== TCP Close/disconnect command
	case 'C':
		os_printf("TCP closing chan %d\n", chan);
		tci = tcpConn+chan;
		if (tci->state > TCP_idle) {
			tci->state = TCP_idle; // hackish...
			espconn_disconnect(tci->conn);
		}
		break;

	}
	return false;
}

