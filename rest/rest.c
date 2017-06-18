// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt
//
// Adapted from: github.com/tuanpmt/esp_bridge, Created on: Mar 4, 2015, Author: Minh

#include "esp8266.h"
#include "c_types.h"
#include "ip_addr.h"
#include "rest.h"
#include "cmd.h"

#ifdef REST_DBG
#define DBG_REST(format, ...) os_printf(format, ## __VA_ARGS__)
#else
#define DBG_REST(format, ...) do { } while(0)
#endif

typedef enum {
  HEADER_GENERIC = 0,
  HEADER_CONTENT_TYPE,
  HEADER_USER_AGENT
} HEADER_TYPE;

typedef struct {
  char           *host;
  uint32_t       port;
  uint32_t       security;
  ip_addr_t      ip;
  struct espconn *pCon;
  char           *header;
  char           *data;
  uint16_t       data_len;
  uint16_t       data_sent;
  char           *content_type;
  char           *user_agent;
  uint32_t       resp_cb;
} RestClient;


// Connection pool for REST clients. Attached MCU's just call REST_setup and this allocates
// a connection, They never call any 'free' and given that the attached MCU could restart at
// any time, we cannot really rely on the attached MCU to call 'free' ever, so better do without.
// Instead, we allocate a fixed pool of connections an round-robin. What this means is that the
// attached MCU should really use at most as many REST connections as there are slots in the pool.
#define MAX_REST 4
static RestClient restClient[MAX_REST];
static uint8_t restNum = 0xff; // index into restClient for next slot to allocate
#define REST_CB 0xbeef0000 // fudge added to callback for arduino so we can detect problems

// Receive HTTP response - this hacky function assumes that the full response is received in
// one go. Sigh...
static void ICACHE_FLASH_ATTR
tcpclient_recv(void *arg, char *pdata, unsigned short len) {
  struct espconn *pCon = (struct espconn*)arg;
  RestClient *client = (RestClient *)pCon->reverse;

  // parse status line
  int pi = 0;
  int16_t code = -1;
  char statusCode[4] = "\0\0\0\0";
  int statusLen = 0;
  bool inStatus = false;
  while (pi < len) {
    if (pdata[pi] == '\n') {
      // end of status line
      if (code == -1) code = 502; // BAD GATEWAY
      break;
    } else if (pdata[pi] == ' ') {
      if (inStatus) code = atoi(statusCode);
      inStatus = !inStatus;
    } else if (inStatus) {
      if (statusLen < 3) statusCode[statusLen] = pdata[pi];
      statusLen++;
    }
    pi++;
  }

  // parse header, all this does is look for the end of the header
  bool currentLineIsBlank = false;
  while (pi < len) {
    if (pdata[pi] == '\n') {
      if (currentLineIsBlank) {
        // body is starting
        pi++;
        break;
      }
      currentLineIsBlank = true;
    } else if (pdata[pi] != '\r') {
      currentLineIsBlank = false;
    }
    pi++;
  }
  //if (pi < len && pdata[pi] == '\r') pi++; // hacky!

  // collect body and send it
  int body_len = len-pi;
  DBG_REST("REST: status=%d, body=%d\n", code, body_len);
  if (pi == len) {
    cmdResponseStart(CMD_RESP_CB, client->resp_cb, 1);
    cmdResponseBody(&code, sizeof(code));
    cmdResponseEnd();
  } else {
    cmdResponseStart(CMD_RESP_CB, client->resp_cb, 2);
    cmdResponseBody(&code, sizeof(code));
    cmdResponseBody(pdata+pi, body_len>100?100:body_len);
    cmdResponseEnd();
#if 0
    os_printf("REST: body=");
    for (int j=pi; j<len; j++) os_printf(" %02x", pdata[j]);
    os_printf("\n");
#endif
  }

  //if(client->security)
  //  espconn_secure_disconnect(client->pCon);
  //else
    espconn_disconnect(client->pCon);
}

static void ICACHE_FLASH_ATTR
tcpclient_sent_cb(void *arg) {
  struct espconn *pCon = (struct espconn *)arg;
  RestClient* client = (RestClient *)pCon->reverse;
  DBG_REST("REST: Sent\n");
  if (client->data_sent != client->data_len) {
    // we only sent part of the buffer, send the rest
    espconn_sent(client->pCon, (uint8_t*)(client->data+client->data_sent),
          client->data_len-client->data_sent);
    client->data_sent = client->data_len;
  } else {
    // we're done sending, free the memory
    if (client->data) os_free(client->data);
    client->data = 0;
  }
}

static void ICACHE_FLASH_ATTR
tcpclient_discon_cb(void *arg) {
  struct espconn *pespconn = (struct espconn *)arg;
  RestClient* client = (RestClient *)pespconn->reverse;
  // free the data buffer, if we have one
  if (client->data) os_free(client->data);
  client->data = 0;
}

static void ICACHE_FLASH_ATTR
tcpclient_recon_cb(void *arg, sint8 errType) {
  struct espconn *pCon = (struct espconn *)arg;
  RestClient* client = (RestClient *)pCon->reverse;
  os_printf("REST #%d: conn reset, err=%d\n", client-restClient, errType);
  // free the data buffer, if we have one
  if (client->data) os_free(client->data);
  client->data = 0;
}

static void ICACHE_FLASH_ATTR
tcpclient_connect_cb(void *arg) {
  struct espconn *pCon = (struct espconn *)arg;
  RestClient* client = (RestClient *)pCon->reverse;
  DBG_REST("REST #%d: connected\n", client-restClient);
  espconn_regist_disconcb(client->pCon, tcpclient_discon_cb);
  espconn_regist_recvcb(client->pCon, tcpclient_recv);
  espconn_regist_sentcb(client->pCon, tcpclient_sent_cb);

  client->data_sent = client->data_len <= 1400 ? client->data_len : 1400;
  DBG_REST("REST #%d: sending %d\n", client-restClient, client->data_sent);
  //if(client->security){
  //  espconn_secure_sent(client->pCon, client->data, client->data_sent);
  //}
  //else{
    espconn_sent(client->pCon, (uint8_t*)client->data, client->data_sent);
  //}
}

static void ICACHE_FLASH_ATTR
rest_dns_found(const char *name, ip_addr_t *ipaddr, void *arg) {
  struct espconn *pConn = (struct espconn *)arg;
  RestClient* client = (RestClient *)pConn->reverse;

  if(ipaddr == NULL) {
    os_printf("REST DNS: Got no ip, try to reconnect\n");
    return;
  }
  DBG_REST("REST DNS: found ip %d.%d.%d.%d\n",
      *((uint8 *) &ipaddr->addr),
      *((uint8 *) &ipaddr->addr + 1),
      *((uint8 *) &ipaddr->addr + 2),
      *((uint8 *) &ipaddr->addr + 3));
  if(client->ip.addr == 0 && ipaddr->addr != 0) {
    os_memcpy(client->pCon->proto.tcp->remote_ip, &ipaddr->addr, 4);
#ifdef CLIENT_SSL_ENABLE
    if(client->security) {
      espconn_secure_connect(client->pCon);
    } else
#endif
    espconn_connect(client->pCon);
    DBG_REST("REST: connecting...\n");
  }
}

void ICACHE_FLASH_ATTR
REST_Setup(CmdPacket *cmd) {
  CmdRequest req;
  uint16_t port;
  uint8_t security;
  int32_t err = -1; // error code in case of failure

  // start parsing the command
  cmdRequest(&req, cmd);
  if(cmdGetArgc(&req) != 3) goto fail;
  err--;

  // get the hostname
  uint16_t len = cmdArgLen(&req);
  if (len > 128) goto fail; // safety check
  err--;
  uint8_t *rest_host = (uint8_t*)os_zalloc(len + 1);
  if (cmdPopArg(&req, rest_host, len)) goto fail;
  err--;
  rest_host[len] = 0;

  // get the port
  if (cmdPopArg(&req, (uint8_t*)&port, 2)) {
    os_free(rest_host);
    goto fail;
  }
  err--;

  // get the security mode
  if (cmdPopArg(&req, (uint8_t*)&security, 1)) {
    os_free(rest_host);
    goto fail;
  }
  err--;

  // clear connection structures the first time
  if (restNum == 0xff) {
    os_memset(restClient, 0, MAX_REST * sizeof(RestClient));
    restNum = 0;
  }

  // allocate a connection structure
  RestClient *client = restClient + restNum;
  uint8_t clientNum = restNum;
  restNum = (restNum+1)%MAX_REST;

  // free any data structure that may be left from a previous connection
  if (client->header) os_free(client->header);
  if (client->content_type) os_free(client->content_type);
  if (client->user_agent) os_free(client->user_agent);
  if (client->data) os_free(client->data);
  if (client->pCon) {
    if (client->pCon->proto.tcp) os_free(client->pCon->proto.tcp);
    os_free(client->pCon);
  }
  os_memset(client, 0, sizeof(RestClient));
  DBG_REST("REST: setup #%d host=%s port=%d security=%d\n", clientNum, rest_host, port, security);

  client->resp_cb = cmd->value;

  client->host = (char *)rest_host;
  client->port = port;
  client->security = security;

  client->header = (char*)os_zalloc(4);
  client->header[0] = 0;

  client->content_type = (char*)os_zalloc(22);
  os_sprintf((char *)client->content_type, "x-www-form-urlencoded");

  client->user_agent = (char*)os_zalloc(9);
  os_sprintf((char *)client->user_agent, "esp-link");

  client->pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
  client->pCon->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));

  client->pCon->type = ESPCONN_TCP;
  client->pCon->state = ESPCONN_NONE;
  client->pCon->proto.tcp->local_port = espconn_port();
  client->pCon->proto.tcp->remote_port = client->port;

  client->pCon->reverse = client;

  cmdResponseStart(CMD_RESP_V, clientNum, 0);
  cmdResponseEnd();
  return;

fail:
  cmdResponseStart(CMD_RESP_V, err, 0);
  cmdResponseEnd();
  return;
}

void ICACHE_FLASH_ATTR
REST_SetHeader(CmdPacket *cmd) {
  CmdRequest req;
  cmdRequest(&req, cmd);

  if(cmdGetArgc(&req) != 2) return;

  // Get client
  uint32_t clientNum = cmd->value;
  RestClient *client = restClient + (clientNum % MAX_REST);

  // Get header selector
  uint8_t header_index;
  if (cmdPopArg(&req, &header_index, 1)) return;

  // Get header value
  uint16_t len = cmdArgLen(&req);
  if (len > 256) return; //safety check
  switch(header_index) {
  case HEADER_GENERIC:
    if(client->header) os_free(client->header);
    client->header = (char*)os_zalloc(len + 3);
    cmdPopArg(&req, (uint8_t*)client->header, len);
    client->header[len] = '\r';
    client->header[len+1] = '\n';
    client->header[len+2] = 0;
    DBG_REST("REST: Set header: %s\r\n", client->header);
    break;
  case HEADER_CONTENT_TYPE:
    if(client->content_type) os_free(client->content_type);
    client->content_type = (char*)os_zalloc(len + 3);
    cmdPopArg(&req, (uint8_t*)client->content_type, len);
    client->content_type[len] = '\r';
    client->content_type[len+1] = '\n';
    client->content_type[len+2] = 0;
    DBG_REST("REST: Set content_type: %s\r\n", client->content_type);
    break;
  case HEADER_USER_AGENT:
    if(client->user_agent) os_free(client->user_agent);
    client->user_agent = (char*)os_zalloc(len + 3);
    cmdPopArg(&req, (uint8_t*)client->user_agent, len);
    client->user_agent[len] = '\r';
    client->user_agent[len+1] = '\n';
    client->user_agent[len+2] = 0;
    DBG_REST("REST: Set user_agent: %s\r\n", client->user_agent);
    break;
  }
}

void ICACHE_FLASH_ATTR
REST_Request(CmdPacket *cmd) {
  CmdRequest req;
  cmdRequest(&req, cmd);
  DBG_REST("REST: request");
  if (cmd->argc != 2 && cmd->argc != 3) return;

  // Get client
  uint32_t clientNum = cmd->value;
  RestClient *client = restClient + (clientNum % MAX_REST);
  DBG_REST(" #%d", clientNum);

  // Get HTTP method
  uint16_t len = cmdArgLen(&req);
  if (len > 15) goto fail;
  char method[16];
  cmdPopArg(&req, method, len);
  method[len] = 0;
  DBG_REST(" method=%s", method);

  // Get HTTP path
  len = cmdArgLen(&req);
  if (len > 1023) goto fail;
  char path[1024];
  cmdPopArg(&req, path, len);
  path[len] = 0;
  DBG_REST(" path=%s", path);

  // Get HTTP body
  uint32_t realLen = 0;
  if (cmdGetArgc(&req) == 2) {
    realLen = 0;
  } else {
    realLen = cmdArgLen(&req);
    if (realLen > 2048) goto fail;
  }
  DBG_REST(" bodyLen=%d", realLen);

  // we need to allocate memory for the header plus the body. First we count the length of the
  // header (including some extra counted "%s" and then we add the body length. We allocate the
  // whole shebang and copy everything into it.
  // BTW, use http/1.0 to avoid responses with transfer-encoding: chunked
  char *headerFmt = "%s %s HTTP/1.0\r\n"
                    "Host: %s\r\n"
                    "%s"
                    "Content-Length: %d\r\n"
                    "Connection: close\r\n"
                    "Content-Type: %s\r\n"
                    "User-Agent: %s\r\n\r\n";
  uint16_t headerLen = strlen(headerFmt) + strlen(method) + strlen(path) + strlen(client->host) +
      strlen(client->header) + strlen(client->content_type) + strlen(client->user_agent);
  DBG_REST(" hdrLen=%d", headerLen);
  if (client->data) os_free(client->data);
  client->data = (char*)os_zalloc(headerLen + realLen);
  if (client->data == NULL) goto fail;
  DBG_REST(" totLen=%d data=%p", headerLen + realLen, client->data);
  client->data_len = os_sprintf((char*)client->data, headerFmt, method, path, client->host,
      client->header, realLen, client->content_type, client->user_agent);
  DBG_REST(" hdrLen=%d", client->data_len);

  if (realLen > 0) {
    cmdPopArg(&req, client->data + client->data_len, realLen);
    client->data_len += realLen;
  }
  DBG_REST("\n");

  //DBG_REST("REST request: %s", (char*)client->data);

  //DBG_REST("REST: pCon state=%d\n", client->pCon->state);
  client->pCon->state = ESPCONN_NONE;
  espconn_regist_connectcb(client->pCon, tcpclient_connect_cb);
  espconn_regist_reconcb(client->pCon, tcpclient_recon_cb);

  if(UTILS_StrToIP((char *)client->host, &client->pCon->proto.tcp->remote_ip)) {
    DBG_REST("REST: Connect to ip %s:%d\n",client->host, client->port);
    //if(client->security){
    //  espconn_secure_connect(client->pCon);
    //}
    //else {
      espconn_connect(client->pCon);
    //}
  } else {
    DBG_REST("REST: Connect to host %s:%d\n", client->host, client->port);
    espconn_gethostbyname(client->pCon, (char *)client->host, &client->ip, rest_dns_found);
  }

  return;

fail:
  DBG_REST("\n");
}
