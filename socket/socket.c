// Copyright 2016 by BeeGee, see LICENSE.txt
//
// Adapted from: github.com/tuanpmt/esp_bridge, Created on: Mar 4, 2015, Author: Minh
// Adapted from: rest.c, Author: Thorsten von Eicken

#include "esp8266.h"
#include "c_types.h"
#include "ip_addr.h"
#include "socket.h"

#define SOCK_DBG

#ifdef SOCK_DBG
#define DBG_SOCK(format, ...) os_printf(format, ## __VA_ARGS__)
#else
#define DBG_SOCK(format, ...) do { } while(0)
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
} SocketClient;


// Connection pool for TCP/UDP socket clients/servers. Attached MCU's just call SOCKET_setup and this allocates
// a connection, They never call any 'free' and given that the attached MCU could restart at
// any time, we cannot really rely on the attached MCU to call 'free' ever, so better do without.
// Instead, we allocate a fixed pool of connections an round-robin. What this means is that the
// attached MCU should really use at most as many SOCKET connections as there are slots in the pool.
#define MAX_SOCKET 4
#define MAX_RECEIVE_PACKET_LENGTH 100

static SocketClient socketClient[MAX_SOCKET];
static uint8_t socketNum = 0xff; // index into socketClient for next slot to allocate

// Any incoming data?
static void ICACHE_FLASH_ATTR
socketclient_recv_cb(void *arg, char *pusrdata, unsigned short length) {
	struct espconn *pCon = (struct espconn *)arg;
	SocketClient* client = (SocketClient *)pCon->reverse;

	uint8_t clientNum = client->conn_num;
	uint8_t cb_type = USERCB_RECV;
	DBG_SOCK("SOCKET #%d: Received %d bytes: %s\n", client-socketClient, length, pusrdata);
	
	unsigned short position = 0;
	do
	{
		unsigned short msgLen = length - position;
		if( msgLen > MAX_RECEIVE_PACKET_LENGTH )
			msgLen = MAX_RECEIVE_PACKET_LENGTH;
		
		cmdResponseStart(CMD_RESP_CB, client->resp_cb, 4);
		cmdResponseBody(&cb_type, 1);	
		cmdResponseBody(&clientNum, 1);
		cmdResponseBody(&msgLen, 2);
		cmdResponseBody(pusrdata + position, msgLen);
		cmdResponseEnd();
		
		position += msgLen;
	}while(position < length );
	
	if (client->sock_mode != SOCKET_TCP_SERVER) { // We don't wait for a response
		DBG_SOCK("SOCKET #%d: disconnect after receiving\n", client-socketClient);
		espconn_disconnect(client->pCon); // disconnect from the server
	}
}

// Data is sent
static void ICACHE_FLASH_ATTR
socketclient_sent_cb(void *arg) {
	struct espconn *pCon = (struct espconn *)arg;
	SocketClient* client = (SocketClient *)pCon->reverse;

	uint8_t clientNum = client->conn_num;
	uint8_t cb_type = USERCB_SENT;
	DBG_SOCK("SOCKET #%d: Sent\n", client-socketClient);
	sint16 sentDataLen = client->data_sent;
	if (client->data_sent != client->data_len) 
	{
		// we only sent part of the buffer, send the rest
		uint16_t data_left = client->data_len - client->data_sent;
		if (data_left  > 1400) // we have more than 1400 bytes left
		{ 
			data_left = 1400;
			espconn_sent(client->pCon, (uint8_t*)(client->data+client->data_sent), 1400 );
		}
		espconn_sent(client->pCon, (uint8_t*)(client->data+client->data_sent), data_left );
		client->data_sent += data_left;
	}
	else
	{
		// we're done sending, free the memory
		if (client->data) os_free(client->data);
		client->data = 0;

		if (client->sock_mode == SOCKET_TCP_CLIENT) { // We don't wait for a response
			DBG_SOCK("SOCKET #%d: disconnect after sending\n", clientNum);
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
socketclient_discon_cb(void *arg) {
	struct espconn *pespconn = (struct espconn *)arg;
	SocketClient* client = (SocketClient *)pespconn->reverse;

	uint8_t clientNum = client->conn_num;
	uint8_t cb_type = USERCB_CONN;
	sint16 _status = CONNSTAT_DIS;
	DBG_SOCK("SOCKET #%d: Disconnect\n", clientNum);
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
socketclient_recon_cb(void *arg, sint8 errType) {
	struct espconn *pCon = (struct espconn *)arg;
	SocketClient* client = (SocketClient *)pCon->reverse;

	uint8_t clientNum = client->conn_num;
	uint8_t cb_type = USERCB_RECO;
	sint16 _errType = errType;
	os_printf("SOCKET #%d: conn reset, err=%d\n", clientNum, _errType);
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
socketclient_connect_cb(void *arg) {
	struct espconn *pCon = (struct espconn *)arg;
	SocketClient* client = (SocketClient *)pCon->reverse;

	uint8_t clientNum = client->conn_num;
	uint8_t cb_type = USERCB_CONN;
	sint16 _status = CONNSTAT_CON;
	DBG_SOCK("SOCKET #%d: connected socket mode = %d\n", clientNum, client->sock_mode);
	espconn_regist_disconcb(client->pCon, socketclient_discon_cb);
	espconn_regist_recvcb(client->pCon, socketclient_recv_cb);
	espconn_regist_sentcb(client->pCon, socketclient_sent_cb);

	DBG_SOCK("SOCKET #%d: sending %d\n", clientNum, client->data_sent);
	if (client->sock_mode != SOCKET_TCP_SERVER) { // Send data after established connection only in client mode
		client->data_sent = client->data_len <= 1400 ? client->data_len : 1400;
		DBG_SOCK("SOCKET #%d: sending %d\n", clientNum, client->data_sent);
		espconn_send(client->pCon, (uint8_t*)client->data, client->data_sent);
	}

	cmdResponseStart(CMD_RESP_CB, client->resp_cb, 3);
	cmdResponseBody(&cb_type, 1);	
	cmdResponseBody(&clientNum, 1);
	cmdResponseBody(&_status, 2);
	cmdResponseEnd();
}

static void ICACHE_FLASH_ATTR
socket_dns_found(const char *name, ip_addr_t *ipaddr, void *arg) {
	struct espconn *pConn = (struct espconn *)arg;
	SocketClient* client = (SocketClient *)pConn->reverse;
	uint8_t clientNum = client->conn_num;

	if(ipaddr == NULL) {
		sint16 _errType = ESPCONN_RTE; //-4;   
		uint8_t cb_type = USERCB_RECO; // use Routing problem or define a new one
		os_printf("SOCKET #%d DNS: Got no ip, report error\n", clientNum);
		cmdResponseStart(CMD_RESP_CB, client->resp_cb, 3);    
		cmdResponseBody(&cb_type, 2); // Same as connection reset?? or define a new one
		cmdResponseBody(&clientNum, 1);    
		cmdResponseBody(&_errType, 2);    
		cmdResponseEnd();    
		return;
	}
	DBG_SOCK("SOCKET #%d DNS: found ip %d.%d.%d.%d\n",
			clientNum,
			*((uint8 *) &ipaddr->addr),
			*((uint8 *) &ipaddr->addr + 1),
			*((uint8 *) &ipaddr->addr + 2),
			*((uint8 *) &ipaddr->addr + 3));
	if(client->ip.addr == 0 && ipaddr->addr != 0) {
		os_memcpy(client->pCon->proto.tcp->remote_ip, &ipaddr->addr, 4);
		espconn_connect(client->pCon);
		DBG_SOCK("SOCKET #%d: connecting...\n", clientNum);
	}
}

void ICACHE_FLASH_ATTR
SOCKET_Setup(CmdPacket *cmd) {
	CmdRequest req;
	uint16_t port;
	uint8_t sock_mode;
	int32_t err = -1; // error code in case of failure

	// start parsing the command
	cmdRequest(&req, cmd);
	if(cmdGetArgc(&req) != 3) {
		DBG_SOCK("SOCKET Setup parse command failure: (cmdGetArgc(&req) != 3)\n");
		goto fail;
	}
	err--;

	// get the hostname (IP address)
	uint16_t len = cmdArgLen(&req);
	if (len > 128) {
		DBG_SOCK("SOCKET Setup parse command failure: hostname longer than 128 characters\n");
		goto fail; // safety check
	}
	err--;
	uint8_t *socket_host = (uint8_t*)os_zalloc(len + 1);
	if (socket_host == NULL) {
		DBG_SOCK("SOCKET Setup failed to alloc memory for socket_host\n");
		goto fail;
	}
	if (cmdPopArg(&req, socket_host, len)) {
		DBG_SOCK("SOCKET Setup parse command failure: (cmdPopArg(&req, socket_host, len))\n");
		goto fail;
	}
	err--;
	socket_host[len] = 0;

	// get the port
	if (cmdPopArg(&req, (uint8_t*)&port, 2)) {
		DBG_SOCK("SOCKET Setup parse command failure: cannot get port\n");
		os_free(socket_host);
		goto fail;
	}
	err--;

	// get the socket mode
	if (cmdPopArg(&req, (uint8_t*)&sock_mode, 1)) {
		DBG_SOCK("SOCKET Setup parse command failure: cannot get mode\n");
		os_free(socket_host);
		goto fail;
	}
	err--;
	DBG_SOCK("SOCKET Setup listener flag\n");

	// clear connection structures the first time
	if (socketNum == 0xff) {
		os_memset(socketClient, 0, MAX_SOCKET * sizeof(SocketClient));
		socketNum = 0;
	}

	// allocate a connection structure
	SocketClient *client = socketClient + socketNum;
	uint8_t clientNum = socketNum;
	socketNum = (socketNum+1)%MAX_SOCKET;

	// free any data structure that may be left from a previous connection
	if (client->data) os_free(client->data);
	if (client->pCon) {
		if (sock_mode != SOCKET_UDP) {
			if (client->pCon->proto.tcp) os_free(client->pCon->proto.tcp);
		} else {
			if (client->pCon->proto.udp) os_free(client->pCon->proto.udp);
		}
		os_free(client->pCon);
	}
	os_memset(client, 0, sizeof(SocketClient));
	DBG_SOCK("SOCKET #%d: Setup host=%s port=%d \n", clientNum, socket_host, port);

	client->sock_mode = sock_mode;
	client->resp_cb = cmd->value;
	client->conn_num = clientNum;

	client->host = (char *)socket_host;
	client->port = port;
	
	if (sock_mode == SOCKET_UDP) {
		wifi_set_broadcast_if(STATIONAP_MODE);
	}

	client->pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
	if (client->pCon == NULL) {
		DBG_SOCK("SOCKET #%d: Setup failed to alloc memory for client_pCon\n", clientNum);
		goto fail;
	}

	if (sock_mode != SOCKET_UDP) {
		client->pCon->type = ESPCONN_TCP;
		client->pCon->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
		if (client->pCon->proto.tcp == NULL) {
			DBG_SOCK("SOCKET #%d: Setup failed to alloc memory for client->pCon->proto.tcp\n", clientNum);
			goto fail;
		}
	} else {
		client->pCon->type = ESPCONN_UDP;
		client->pCon->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
		if (client->pCon->proto.udp == NULL) {
			DBG_SOCK("SOCKET #%d: Setup failed to alloc memory for client->pCon->proto.udp\n", clientNum);
			goto fail;
		}
	}
	client->pCon->state = ESPCONN_NONE;

	os_memcpy(client->host, socket_host, 4);
	if (sock_mode != SOCKET_UDP) {
		client->pCon->proto.tcp->remote_port = client->port;
		client->pCon->proto.tcp->local_port = client->port; // espconn_port();	
	} else {
		client->pCon->proto.udp->remote_port = client->port;
		client->pCon->proto.udp->local_port = client->port;
	}

	client->pCon->reverse = client;

	espconn_regist_sentcb(client->pCon, socketclient_sent_cb);
	espconn_regist_recvcb(client->pCon, socketclient_recv_cb);
	if (sock_mode == SOCKET_UDP) {
		DBG_SOCK("SOCKET #%d: Create connection to ip %s:%d\n", clientNum, client->host, client->port);
		
		if(UTILS_StrToIP((char *)client->host, &client->pCon->proto.udp->remote_ip)) {
			espconn_create(client->pCon);
		} else {
			DBG_SOCK("SOCKET #%d: failed to copy remote_ip to &client->pCon->proto.udp->remote_ip\n", clientNum);
			goto fail;
		}
	} else {
		espconn_regist_reconcb(client->pCon, socketclient_recon_cb);
		if (client->sock_mode == SOCKET_TCP_SERVER) { // Server mode?
			DBG_SOCK("SOCKET #%d: Enable server mode on port%d\n", clientNum, client->port);
			espconn_accept(client->pCon);
			espconn_regist_connectcb(client->pCon, socketclient_connect_cb);
		}
	}
	
	cmdResponseStart(CMD_RESP_V, clientNum, 0);
	cmdResponseEnd();
	DBG_SOCK("SOCKET #%d: setup finished\n", clientNum);
	return;

fail:
	cmdResponseStart(CMD_RESP_V, err, 0);
	cmdResponseEnd();
	return;
}

void ICACHE_FLASH_ATTR
SOCKET_Send(CmdPacket *cmd) {
	CmdRequest req;
	cmdRequest(&req, cmd);
	
	// Get client
	uint32_t clientNum = cmd->value;
	SocketClient *client = socketClient + (clientNum % MAX_SOCKET);
	DBG_SOCK("SOCKET #%d: send", clientNum);

	if (cmd->argc != 1 && cmd->argc != 2) {
		DBG_SOCK("\nSOCKET #%d: send - wrong number of arguments\n", clientNum);
		return;
	}
	
	// Get data to sent
	client->data_len = cmdArgLen(&req);
	DBG_SOCK(" dataLen=%d", client->data_len);

	if (client->data) os_free(client->data);
	client->data = (char*)os_zalloc(client->data_len);
	if (client->data == NULL) {
		DBG_SOCK("\nSOCKET #%d failed to alloc memory for client->data\n", clientNum);
		goto fail;
	}
	cmdPopArg(&req, client->data, client->data_len);
	DBG_SOCK(" socketData=%s", client->data);

	// client->data_len = os_sprintf((char*)client->data, socketDataSet, socketData);
	
	DBG_SOCK("\n");

	DBG_SOCK("SOCKET #%d: Create connection to ip %s:%d\n", clientNum, client->host, client->port);

	if (client->sock_mode == SOCKET_TCP_SERVER) { // In TCP server mode we should be connected already and send the data immediately
		remot_info *premot = NULL;
		if (espconn_get_connection_info(client->pCon,&premot,0) == ESPCONN_OK){
			for (uint8 count = 0; count < client->pCon->link_cnt; count ++){
				client->pCon->proto.tcp->remote_port = premot[count].remote_port;
          
				client->pCon->proto.tcp->remote_ip[0] = premot[count].remote_ip[0];
				client->pCon->proto.tcp->remote_ip[1] = premot[count].remote_ip[1];
				client->pCon->proto.tcp->remote_ip[2] = premot[count].remote_ip[2];
				client->pCon->proto.tcp->remote_ip[3] = premot[count].remote_ip[3];
				DBG_SOCK("SOCKET #%d: connected to %d.%d.%d.%d:%d\n", 
					clientNum,
					client->pCon->proto.tcp->remote_ip[0],
					client->pCon->proto.tcp->remote_ip[1],
					client->pCon->proto.tcp->remote_ip[2],
					client->pCon->proto.tcp->remote_ip[3],
					client->pCon->proto.tcp->remote_port
					);
			}
			client->data_sent = client->data_len <= 1400 ? client->data_len : 1400;
			DBG_SOCK("SOCKET #%d: Server sending %d\n", clientNum, client->data_sent);
			espconn_send(client->pCon, (uint8_t*)client->data, client->data_sent);
		}
	} else if (client->sock_mode != SOCKET_UDP) { // In TCP client mode we connect and send the data from the connected callback
		espconn_regist_connectcb(client->pCon, socketclient_connect_cb);
		
		if(UTILS_StrToIP((char *)client->host, &client->pCon->proto.tcp->remote_ip)) {
			DBG_SOCK("SOCKET #%d: Connect to ip %s:%d\n", clientNum, client->host, client->port);
			espconn_connect(client->pCon);
		} else {
			DBG_SOCK("SOCKET #%d: Connect to host %s:%d\n", clientNum, client->host, client->port);
			espconn_gethostbyname(client->pCon, (char *)client->host, &client->ip, socket_dns_found);
		} 
	} else { // in UDP socket mode we send the data immediately
		client->data_sent = client->data_len <= 1400 ? client->data_len : 1400;
		DBG_SOCK("SOCKET #%d: sending %d bytes: %s\n", clientNum, client->data_sent, client->data);
		espconn_sent(client->pCon, (uint8_t*)client->data, client->data_sent);
	}

	return;

fail:
	DBG_SOCK("\n");
}
