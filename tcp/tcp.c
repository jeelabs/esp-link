// Copyright 2016 by BeeGee, see LICENSE.txt
//
// Adapted from: github.com/tuanpmt/esp_bridge, Created on: Mar 4, 2015, Author: Minh
// Adapted from: rest.c, Author: Thorsten von Eicken

#include "esp8266.h"
#include "c_types.h"
#include "ip_addr.h"
#include "tcp.h"
#include "cmd.h"

#define TCP_DBG

#ifdef TCP_DBG
#define DBG_TCP(format, ...) os_printf(format, ## __VA_ARGS__)
#else
#define DBG_TCP(format, ...) do { } while(0)
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
	uint8_t			sock_mode;
} TcpClient;


// Connection pool for TCP socket clients. Attached MCU's just call TCP_setup and this allocates
// a connection, They never call any 'free' and given that the attached MCU could restart at
// any time, we cannot really rely on the attached MCU to call 'free' ever, so better do without.
// Instead, we allocate a fixed pool of connections an round-robin. What this means is that the
// attached MCU should really use at most as many TCP connections as there are slots in the pool.
#define MAX_TCP 4
static TcpClient tcpClient[MAX_TCP];
static uint8_t tcpNum = 0xff; // index into tcpClient for next slot to allocate

// Any incoming data?
static void ICACHE_FLASH_ATTR
tcpclient_recv_cb(void *arg, char *pusrdata, unsigned short length) {
	struct espconn *pCon = (struct espconn *)arg;
	TcpClient* client = (TcpClient *)pCon->reverse;

	uint8_t clientNum = client->conn_num;
	uint8_t cb_type = 1;
	DBG_TCP("TCP #%d: Received %d bytes: %s\n", client-tcpClient, length, pusrdata);
	cmdResponseStart(CMD_RESP_CB, client->resp_cb, 4);
	cmdResponseBody(&cb_type, 1);	
	cmdResponseBody(&clientNum, 1);
	cmdResponseBody(&length, 2);
	cmdResponseBody(pusrdata, length);
	cmdResponseEnd();
	
	if (client->sock_mode != SOCKET_SERVER) { // We don't wait for a response
		DBG_TCP("TCP #%d: disconnect after receiving\n", client-tcpClient);
		espconn_disconnect(client->pCon); // disconnect from the server
	}
}

// Data is sent
static void ICACHE_FLASH_ATTR
tcpclient_sent_cb(void *arg) {
	struct espconn *pCon = (struct espconn *)arg;
	TcpClient* client = (TcpClient *)pCon->reverse;

	uint8_t clientNum = client->conn_num;
	uint8_t cb_type = 0;
	DBG_TCP("TCP #%d: Sent\n", client-tcpClient);
	sint16 sentDataLen = client->data_sent;
	if (client->data_sent != client->data_len) {
		// we only sent part of the buffer, send the rest
		espconn_send(client->pCon, (uint8_t*)(client->data+client->data_sent),
					client->data_len-client->data_sent);
		client->data_sent = client->data_len;
	} else {
		// we're done sending, free the memory
		if (client->data) os_free(client->data);
		client->data = 0;

		if (client->sock_mode == SOCKET_CLIENT) { // We don't wait for a response
			DBG_TCP("TCP #%d: disconnect after sending\n", client-tcpClient);
			espconn_disconnect(client->pCon);
		}

		cmdResponseStart(CMD_RESP_CB, client->resp_cb, 3);
		cmdResponseBody(&cb_type, 1);	
		cmdResponseBody(&clientNum, 1);
		cmdResponseBody(&sentDataLen, 2);
		cmdResponseEnd();
	}
}

// Connection is disconnected
static void ICACHE_FLASH_ATTR
tcpclient_discon_cb(void *arg) {
	struct espconn *pespconn = (struct espconn *)arg;
	TcpClient* client = (TcpClient *)pespconn->reverse;

	uint8_t clientNum = client->conn_num;
	uint8_t cb_type = 3;
	sint16 _status = 0;
	DBG_TCP("TCP #%d: Disconnect\n", client-tcpClient);
	// free the data buffer, if we have one
	if (client->data) os_free(client->data);
	client->data = 0;
	cmdResponseStart(CMD_RESP_CB, client->resp_cb, 3);
	cmdResponseBody(&cb_type, 1);	
	cmdResponseBody(&clientNum, 1);
	cmdResponseBody(&_status, 2);
	cmdResponseEnd();
}

// Connection was reset
static void ICACHE_FLASH_ATTR
tcpclient_recon_cb(void *arg, sint8 errType) {
	struct espconn *pCon = (struct espconn *)arg;
	TcpClient* client = (TcpClient *)pCon->reverse;

	uint8_t clientNum = client->conn_num;
	uint8_t cb_type = 2;
	sint16 _errType = errType;
	os_printf("TCP #%d: conn reset, err=%d\n", client-tcpClient, _errType);
	cmdResponseStart(CMD_RESP_CB, client->resp_cb, 3);
	cmdResponseBody(&cb_type, 1);	
	cmdResponseBody(&clientNum, 1);
	cmdResponseBody(&_errType, 2);
	cmdResponseEnd();
	// free the data buffer, if we have one
	if (client->data) os_free(client->data);
	client->data = 0;
}

// Connection is done
static void ICACHE_FLASH_ATTR
tcpclient_connect_cb(void *arg) {
	struct espconn *pCon = (struct espconn *)arg;
	TcpClient* client = (TcpClient *)pCon->reverse;

	uint8_t clientNum = client->conn_num;
	uint8_t cb_type = 3;
	sint16 _status = 1;
	DBG_TCP("TCP #%d: connected socket mode = %d\n", client-tcpClient, client->sock_mode);
	espconn_regist_disconcb(client->pCon, tcpclient_discon_cb);
	espconn_regist_recvcb(client->pCon, tcpclient_recv_cb);
	espconn_regist_sentcb(client->pCon, tcpclient_sent_cb);

	DBG_TCP("TCP #%d: sending %d\n", client-tcpClient, client->data_sent);
	if (client->sock_mode != SOCKET_SERVER) { // Send data after established connection only in client mode
		client->data_sent = client->data_len <= 1400 ? client->data_len : 1400;
		DBG_TCP("TCP #%d: sending %d\n", client-tcpClient, client->data_sent);
		espconn_send(client->pCon, (uint8_t*)client->data, client->data_sent);
	}

	cmdResponseStart(CMD_RESP_CB, client->resp_cb, 3);
	cmdResponseBody(&cb_type, 1);	
	cmdResponseBody(&clientNum, 1);
	cmdResponseBody(&_status, 2);
	cmdResponseEnd();
}

static void ICACHE_FLASH_ATTR
tcp_dns_found(const char *name, ip_addr_t *ipaddr, void *arg) {
	struct espconn *pConn = (struct espconn *)arg;
	TcpClient* client = (TcpClient *)pConn->reverse;

	if(ipaddr == NULL) {
		os_printf("TCP #%d DNS: Got no ip, try to reconnect\n", client-tcpClient);
		return;
	}
	DBG_TCP("TCP DNS: found ip %d.%d.%d.%d\n",
			*((uint8 *) &ipaddr->addr),
			*((uint8 *) &ipaddr->addr + 1),
			*((uint8 *) &ipaddr->addr + 2),
			*((uint8 *) &ipaddr->addr + 3));
	if(client->ip.addr == 0 && ipaddr->addr != 0) {
		os_memcpy(client->pCon->proto.tcp->remote_ip, &ipaddr->addr, 4);
		espconn_connect(client->pCon);
		DBG_TCP("TCP #%d: connecting...\n", client-tcpClient);
	}
}

void ICACHE_FLASH_ATTR
TCP_Setup(CmdPacket *cmd) {
	CmdRequest req;
	uint16_t port;
	uint8_t sock_mode;
	int32_t err = -1; // error code in case of failure

	// start parsing the command
	cmdRequest(&req, cmd);
	if(cmdGetArgc(&req) != 3) {
		DBG_TCP("TCP Setup parse command failure: (cmdGetArgc(&req) != 3)\n");
		goto fail;
	}
	err--;

	// get the hostname (IP address)
	uint16_t len = cmdArgLen(&req);
	if (len > 128) {
		DBG_TCP("TCP Setup parse command failure: hostname longer than 128 characters\n");
		goto fail; // safety check
	}
	err--;
	uint8_t *tcp_host = (uint8_t*)os_zalloc(len + 1);
	if (tcp_host == NULL) {
		DBG_TCP("TCP Setup failed to alloc memory for tcp_host\n");
		goto fail;
	}
	if (cmdPopArg(&req, tcp_host, len)) {
		DBG_TCP("TCP Setup parse command failure: (cmdPopArg(&req, tcp_host, len))\n");
		goto fail;
	}
	err--;
	tcp_host[len] = 0;

	// get the port
	if (cmdPopArg(&req, (uint8_t*)&port, 2)) {
		DBG_TCP("TCP Setup parse command failure: cannot get port\n");
		os_free(tcp_host);
		goto fail;
	}
	err--;

	// get the socket mode
	if (cmdPopArg(&req, (uint8_t*)&sock_mode, 1)) {
		DBG_TCP("TCP Setup parse command failure: cannot get mode\n");
		os_free(tcp_host);
		goto fail;
	}
	err--;
	DBG_TCP("TCP Setup listener flag\n");

	// clear connection structures the first time
	if (tcpNum == 0xff) {
		os_memset(tcpClient, 0, MAX_TCP * sizeof(TcpClient));
		tcpNum = 0;
	}

	// allocate a connection structure
	TcpClient *client = tcpClient + tcpNum;
	uint8_t clientNum = tcpNum;
	tcpNum = (tcpNum+1)%MAX_TCP;

	// free any data structure that may be left from a previous connection
	if (client->data) os_free(client->data);
	if (client->pCon) {
		if (client->pCon->proto.tcp) os_free(client->pCon->proto.tcp);
		os_free(client->pCon);
	}
	os_memset(client, 0, sizeof(TcpClient));
	DBG_TCP("TCP #%d: Setup host=%s port=%d \n", clientNum, tcp_host, port);

	client->sock_mode = sock_mode;
	client->resp_cb = cmd->value;
	client->conn_num = clientNum;

	client->host = (char *)tcp_host;
	client->port = port;

	client->pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
	if (client->pCon == NULL) {
		DBG_TCP("TCP #%d: Setup failed to alloc memory for client_pCon\n", clientNum);
		goto fail;
	}

	client->pCon->type = ESPCONN_TCP;
	client->pCon->state = ESPCONN_NONE;
	client->pCon->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
	if (client->pCon->proto.tcp == NULL) {
		DBG_TCP("TCP #%d: Setup failed to alloc memory for client->pCon->proto.tcp\n", clientNum);
		goto fail;
	}

	os_memcpy(client->host, tcp_host, 4);
	client->pCon->proto.tcp->remote_port = client->port;
	client->pCon->proto.tcp->local_port = client->port; // espconn_port();

	client->pCon->reverse = client;

	espconn_regist_sentcb(client->pCon, tcpclient_sent_cb);
	espconn_regist_recvcb(client->pCon, tcpclient_recv_cb);
	espconn_regist_reconcb(client->pCon, tcpclient_recon_cb);
	if (client->sock_mode == SOCKET_SERVER) { // Server mode?
		DBG_TCP("TCP #%d: Enable server mode on port%d\n", clientNum, client->port);
		espconn_accept(client->pCon);
		espconn_regist_connectcb(client->pCon, tcpclient_connect_cb);
	}
	
	cmdResponseStart(CMD_RESP_V, clientNum, 0);
	cmdResponseEnd();
	DBG_TCP("TCP #%d: setup finished\n", clientNum);
	return;

fail:
	cmdResponseStart(CMD_RESP_V, err, 0);
	cmdResponseEnd();
	return;
}

void ICACHE_FLASH_ATTR
TCP_Send(CmdPacket *cmd) {
	CmdRequest req;
	cmdRequest(&req, cmd);
	
	// Get client
	uint32_t clientNum = cmd->value;
	TcpClient *client = tcpClient + (clientNum % MAX_TCP);
	DBG_TCP("TCP #%d: send", clientNum);

	if (cmd->argc != 1 && cmd->argc != 2) {
		DBG_TCP("\nTCP #%d: send - wrong number of arguments\n", clientNum);
		return;
	}
	
	// Get data to sent
	uint16_t dataLen = cmdArgLen(&req);
	DBG_TCP(" dataLen=%d", dataLen);
	char tcpData[1024];
	cmdPopArg(&req, tcpData, dataLen);
	tcpData[dataLen] = 0;
	DBG_TCP(" tcpData=%s", tcpData);

	// we need to allocate memory for the data. We copy the message into it
	char *tcpDataSet = "%s";
	
	if (client->data) os_free(client->data);
	client->data = (char*)os_zalloc(dataLen);
	if (client->data == NULL) {
		DBG_TCP("\nTCP #%d failed to alloc memory for client->data\n", clientNum);
		goto fail;
	}
	client->data_len = os_sprintf((char*)client->data, tcpDataSet, tcpData);
	
	DBG_TCP("\n");

	DBG_TCP("TCP #%d: Create connection to ip %s:%d\n", clientNum, client->host, client->port);

	if (client->sock_mode == SOCKET_SERVER) { // In server mode we should be connected already and send the data immediately
		remot_info *premot = NULL;
		if (espconn_get_connection_info(client->pCon,&premot,0) == ESPCONN_OK){
			for (uint8 count = 0; count < client->pCon->link_cnt; count ++){
				client->pCon->proto.tcp->remote_port = premot[count].remote_port;
          
				client->pCon->proto.tcp->remote_ip[0] = premot[count].remote_ip[0];
				client->pCon->proto.tcp->remote_ip[1] = premot[count].remote_ip[1];
				client->pCon->proto.tcp->remote_ip[2] = premot[count].remote_ip[2];
				client->pCon->proto.tcp->remote_ip[3] = premot[count].remote_ip[3];
				DBG_TCP("TCP #%d: connected to %d.%d.%d.%d:%d\n", 
					client-tcpClient,
					client->pCon->proto.tcp->remote_ip[0],
					client->pCon->proto.tcp->remote_ip[1],
					client->pCon->proto.tcp->remote_ip[2],
					client->pCon->proto.tcp->remote_ip[3],
					client->pCon->proto.tcp->remote_port
					);
			}
			client->data_sent = client->data_len <= 1400 ? client->data_len : 1400;
			DBG_TCP("TCP #%d: Server sending %d\n", client-tcpClient, client->data_sent);
			espconn_send(client->pCon, (uint8_t*)client->data, client->data_sent);
		}
	} else {
		espconn_regist_connectcb(client->pCon, tcpclient_connect_cb);
		
		if(UTILS_StrToIP((char *)client->host, &client->pCon->proto.tcp->remote_ip)) {
			DBG_TCP("TCP #%d: Connect to ip %s:%d\n", clientNum, client->host, client->port);
			espconn_connect(client->pCon);
		} else {
			DBG_TCP("TCP #%d: Connect to host %s:%d\n", clientNum, client->host, client->port);
			espconn_gethostbyname(client->pCon, (char *)client->host, &client->ip, tcp_dns_found);
		}
	}

	return;

fail:
	DBG_TCP("\n");
}
