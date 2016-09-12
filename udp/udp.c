// Copyright 2016 by BeeGee, see LICENSE.txt
//
// Adapted from: github.com/tuanpmt/esp_bridge, Created on: Mar 4, 2015, Author: Minh
// Adapted from: rest.c, Author: Thorsten von Eicken

#include "esp8266.h"
#include "c_types.h"
#include "ip_addr.h"
#include "udp.h"
#include "cmd.h"

#define UDP_DBG

#ifdef UDP_DBG
#define DBG_UDP(format, ...) os_printf(format, ## __VA_ARGS__)
#else
#define DBG_UDP(format, ...) do { } while(0)
#endif

typedef struct {
	char			*host;
	uint32_t		port;
	ip_addr_t		ip;
	struct espconn	*pCon;
	char			*data;
	uint16_t		data_len;
	uint16_t		data_sent;
	uint32_t		resp_cb;
	uint8_t			conn_num;
} UdpClient;


// Connection pool for UDP socket clients. Attached MCU's just call UDP_setup and this allocates
// a connection, They never call any 'free' and given that the attached MCU could restart at
// any time, we cannot really rely on the attached MCU to call 'free' ever, so better do without.
// Instead, we allocate a fixed pool of connections an round-robin. What this means is that the
// attached MCU should really use at most as many UDP connections as there are slots in the pool.
#define MAX_UDP 4
static UdpClient udpClient[MAX_UDP];
static uint8_t udpNum = 0xff; // index into udpClient for next slot to allocate

// Any incoming data?
static void ICACHE_FLASH_ATTR
udpclient_recv_cb(void *arg, char *pusrdata, unsigned short length) {
	struct espconn *pCon = (struct espconn *)arg;
	UdpClient* client = (UdpClient *)pCon->reverse;

	uint8_t clientNum = client->conn_num;
	uint8_t cb_type = 1;
	DBG_UDP("UDP #%d: Received %d bytes: %s\n",clientNum, length, pusrdata);
	cmdResponseStart(CMD_RESP_CB, client->resp_cb, 4);
	cmdResponseBody(&cb_type, 1);	
	cmdResponseBody(&clientNum, 1);
	cmdResponseBody(&length, 2);
	cmdResponseBody(pusrdata, length);
	cmdResponseEnd();
}

// Data is sent
static void ICACHE_FLASH_ATTR
udpclient_sent_cb(void *arg) {
	struct espconn *pCon = (struct espconn *)arg;
	UdpClient* client = (UdpClient *)pCon->reverse;

	uint8_t clientNum = client->conn_num;
	uint8_t cb_type = 0;
	DBG_UDP("UDP #%d: Sent\n",clientNum);
	sint16 sentDataLen = client->data_sent;
	if (client->data_sent != client->data_len) {
		// we only sent part of the buffer, send the rest
		espconn_sent(client->pCon, (uint8_t*)(client->data+client->data_sent),
					client->data_len-client->data_sent);
		client->data_sent = client->data_len;
	} else {
		// we're done sending, free the memory
		if (client->data) os_free(client->data);
		client->data = 0;

		cmdResponseStart(CMD_RESP_CB, client->resp_cb, 3);
		cmdResponseBody(&cb_type, 1);	
		cmdResponseBody(&clientNum, 1);
		cmdResponseBody(&sentDataLen, 2);
		cmdResponseEnd();
	}
}

void ICACHE_FLASH_ATTR
UDP_Setup(CmdPacket *cmd) {
	CmdRequest req;
	uint16_t port;
	int32_t err = -1; // error code in case of failure

	// start parsing the command
	cmdRequest(&req, cmd);
	if(cmdGetArgc(&req) != 3) {
		DBG_UDP("UDP Setup parse command failure: (cmdGetArgc(&req) != 3)\n");
		goto fail;
	}
	err--;

	// get the hostname (IP address)
	uint16_t len = cmdArgLen(&req);
	if (len > 128) {
		DBG_UDP("UDP Setup parse command failure: hostname longer than 128 characters\n");
		goto fail; // safety check
	}
	err--;
	uint8_t *udp_host = (uint8_t*)os_zalloc(len + 1);
	if (udp_host == NULL) {
		DBG_UDP("UDP Setup failed to alloc memory for udp_host\n");
		goto fail;
	}
	if (cmdPopArg(&req, udp_host, len)) {
		DBG_UDP("UDP Setup parse command failure: (cmdPopArg(&req, udp_host, len))\n");
		goto fail;
	}
	err--;
	udp_host[len] = 0;

	// get the port
	if (cmdPopArg(&req, (uint8_t*)&port, 2)) {
		DBG_UDP("UDP Setup parse command failure: cannot get port\n");
		os_free(udp_host);
		goto fail;
	}
	err--;

	// clear connection structures the first time
	if (udpNum == 0xff) {
		os_memset(udpClient, 0, MAX_UDP * sizeof(UdpClient));
		udpNum = 0;
	}

	// allocate a connection structure
	UdpClient *client = udpClient + udpNum;
	uint8_t clientNum = udpNum;
	udpNum = (udpNum+1)%MAX_UDP;

	// free any data structure that may be left from a previous connection
	if (client->data) os_free(client->data);
	if (client->pCon) {
		if (client->pCon->proto.udp) os_free(client->pCon->proto.udp);
		os_free(client->pCon);
	}
	os_memset(client, 0, sizeof(UdpClient));
	DBG_UDP("UDP #%d: Setup udp_host=%s port=%d \n", clientNum, udp_host, port);

	client->resp_cb = cmd->value;
	client->conn_num = clientNum;
	
	client->host = (char *)udp_host;
	client->port = port;
	wifi_set_broadcast_if(STATIONAP_MODE);

	client->pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
	if (client->pCon == NULL) {
		DBG_UDP("UDP #%d: Setup failed to alloc memory for client_pCon\n", clientNum);
		goto fail;
	}

	client->pCon->type = ESPCONN_UDP;
	client->pCon->state = ESPCONN_NONE;
	client->pCon->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
	if (client->pCon->proto.udp == NULL) {
		DBG_UDP("UDP #%d: Setup failed to alloc memory for client->pCon->proto.tcp\n", clientNum);
		goto fail;
	}

	os_memcpy(client->host, udp_host, 4);
	client->pCon->proto.udp->remote_port = client->port;
	client->pCon->proto.udp->local_port = client->port;

	client->pCon->reverse = client;

	espconn_regist_sentcb(client->pCon, udpclient_sent_cb);
	espconn_regist_recvcb(client->pCon, udpclient_recv_cb);

	DBG_UDP("UDP #%d: Create connection to ip %s:%d\n", clientNum, client->host, client->port);
	
	if(UTILS_StrToIP((char *)client->host, &client->pCon->proto.tcp->remote_ip)) {
		espconn_create(client->pCon);
	} else {
		DBG_UDP("UDP #%d: failed to copy remote_ip to &client->pCon->proto.tcp->remote_ip\n", clientNum);
		goto fail;
	}


	cmdResponseStart(CMD_RESP_V, clientNum, 0);
	cmdResponseEnd();
	DBG_UDP("UDP #%d: setup finished\n", clientNum);
	return;

fail:
	cmdResponseStart(CMD_RESP_V, err, 0);
	cmdResponseEnd();
	return;
}

void ICACHE_FLASH_ATTR
UDP_Send(CmdPacket *cmd) {
	CmdRequest req;
	cmdRequest(&req, cmd);
	
	// Get client
	uint32_t clientNum = cmd->value;
	UdpClient *client = udpClient + (clientNum % MAX_UDP);
	DBG_UDP("UDP #%d: send", clientNum);

	if (cmd->argc != 1 && cmd->argc != 2) {
		DBG_UDP("\nUDP #%d: send - wrong number of arguments\n", clientNum);
		return;
	}
	
	// Get data to sent
	uint16_t dataLen = cmdArgLen(&req);
	DBG_UDP(" dataLen=%d", dataLen);
	char udpData[1024];
	cmdPopArg(&req, udpData, dataLen);
	udpData[dataLen] = 0;
	DBG_UDP(" udpData=%s", udpData);

	// we need to allocate memory for the data. We copy the message into it
	char *udpDataSet = "%s";
	
	if (client->data) os_free(client->data);
	client->data = (char*)os_zalloc(dataLen);
	if (client->data == NULL) {
		DBG_UDP("\nUDP #%d: failed to alloc memory for client->data\n", clientNum);
		goto fail;
	}
	client->data_len = os_sprintf((char*)client->data, udpDataSet, udpData);
	
	DBG_UDP("\n");

	client->data_sent = client->data_len <= 1400 ? client->data_len : 1400;
	DBG_UDP("UDP #%d: sending %d bytes: %s\n", client-udpClient, client->data_sent, client->data);
	espconn_sent(client->pCon, (uint8_t*)client->data, client->data_sent);

	return;

fail:
	DBG_UDP("\n");
}
