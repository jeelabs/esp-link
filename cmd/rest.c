/*
 * api.c
 *
 *  Created on: Mar 4, 2015
 *      Author: Minh
 */
#include "esp8266.h"
#include "rest.h"
#include "cmd.h"

extern uint8_t ICACHE_FLASH_ATTR UTILS_StrToIP(const char* str, void *ip);

static void ICACHE_FLASH_ATTR
tcpclient_discon_cb(void *arg) {
  //struct espconn *pespconn = (struct espconn *)arg;
  //RestClient* client = (RestClient *)pespconn->reverse;
}

static void ICACHE_FLASH_ATTR
tcpclient_recv(void *arg, char *pdata, unsigned short len) {
  uint8_t currentLineIsBlank = 0;
  uint8_t httpBody = 0;
  uint8_t inStatus = 0;
  char statusCode[4];
  int i = 0, j;
  uint32_t code = 0;
  uint16_t crc;

  struct espconn *pCon = (struct espconn*)arg;
  RestClient *client = (RestClient *)pCon->reverse;

  for(j=0 ;j<len; j++){
    char c = pdata[j];

    if(c == ' ' && !inStatus){
      inStatus = 1;
    }
    if(inStatus && i < 3 && c != ' '){
      statusCode[i] = c;
      i++;
    }
    if(i == 3){
      statusCode[i] = '\0';
      code = atoi(statusCode);
    }

    if(httpBody){
      //only write response if its not null
      uint32_t body_len = len - j;
      os_printf("REST: status=%ld, body=%ld\n", code, body_len);
      if(body_len == 0){
        crc = CMD_ResponseStart(CMD_REST_EVENTS, client->resp_cb, code, 0);
      } else {
        crc = CMD_ResponseStart(CMD_REST_EVENTS, client->resp_cb, code, 1);
        crc = CMD_ResponseBody(crc, (uint8_t*)(pdata+j), body_len);
      }
      CMD_ResponseEnd(crc);
      break;
    } else {
      if (c == '\n' && currentLineIsBlank) {
        httpBody = true;
      }
      if (c == '\n') {
        // you're starting a new line
        currentLineIsBlank = true;
      } else if (c != '\r') {
        // you've gotten a character on the current line
        currentLineIsBlank = false;
      }
    }
  }
  //if(client->security)
  //  espconn_secure_disconnect(client->pCon);
  //else
    espconn_disconnect(client->pCon);

}

static void ICACHE_FLASH_ATTR
tcpclient_sent_cb(void *arg) {
  //struct espconn *pCon = (struct espconn *)arg;
  //RestClient* client = (RestClient *)pCon->reverse;
  os_printf("REST: Sent\n");
}

static void ICACHE_FLASH_ATTR
tcpclient_connect_cb(void *arg) {
  struct espconn *pCon = (struct espconn *)arg;
  RestClient* client = (RestClient *)pCon->reverse;

  espconn_regist_disconcb(client->pCon, tcpclient_discon_cb);
  espconn_regist_recvcb(client->pCon, tcpclient_recv);////////
  espconn_regist_sentcb(client->pCon, tcpclient_sent_cb);///////

  //if(client->security){
  //  espconn_secure_sent(client->pCon, client->data, client->data_len);
  //}
  //else{
    espconn_sent(client->pCon, client->data, client->data_len);
  //}
}

static void ICACHE_FLASH_ATTR
tcpclient_recon_cb(void *arg, sint8 errType) {
  //struct espconn *pCon = (struct espconn *)arg;
  //RestClient* client = (RestClient *)pCon->reverse;
}

static void ICACHE_FLASH_ATTR
rest_dns_found(const char *name, ip_addr_t *ipaddr, void *arg) {
  struct espconn *pConn = (struct espconn *)arg;
  RestClient* client = (RestClient *)pConn->reverse;

  if(ipaddr == NULL) {
    os_printf("REST DNS: Got no ip, try to reconnect\n");
    return;
  }

  os_printf("REST DNS: found ip %d.%d.%d.%d\n",
      *((uint8 *) &ipaddr->addr),
      *((uint8 *) &ipaddr->addr + 1),
      *((uint8 *) &ipaddr->addr + 2),
      *((uint8 *) &ipaddr->addr + 3));

  if(client->ip.addr == 0 && ipaddr->addr != 0) {
    os_memcpy(client->pCon->proto.tcp->remote_ip, &ipaddr->addr, 4);
    //if(client->security){
    //  espconn_secure_connect(client->pCon);
    //}
    //else {
      espconn_connect(client->pCon);
    //}
    os_printf("REST: connecting...\n");
  }
}

uint32_t ICACHE_FLASH_ATTR
REST_Setup(CmdPacket *cmd) {
  CmdRequest req;
  RestClient *client;
  uint8_t *rest_host;
  uint16_t len;
  uint32_t port, security;


  // start parsing the command
  CMD_Request(&req, cmd);
  os_printf("REST: setup argc=%ld\n", CMD_GetArgc(&req));
  if(CMD_GetArgc(&req) != 3)
    return 0;

  // get the hostname
  len = CMD_ArgLen(&req);
  os_printf("REST: len=%d\n", len);
  if (len > 128) return 0; // safety check
  rest_host = (uint8_t*)os_zalloc(len + 1);
  if (CMD_PopArg(&req, rest_host, len)) return 0;
  rest_host[len] = 0;
  os_printf("REST: setup host=%s", rest_host);

  // get the port
  if (CMD_PopArg(&req, (uint8_t*)&port, 4)) {
    os_free(rest_host);
    return 0;
  }
  os_printf(" port=%ld", port);

  // get the security mode
  if (CMD_PopArg(&req, (uint8_t*)&security, 4)) {
    os_free(rest_host);
    return 0;
  }
  os_printf(" security=%ld\n", security);

  // allocate a connection structure
  client = (RestClient*)os_zalloc(sizeof(RestClient));
  os_memset(client, 0, sizeof(RestClient));
  if(client == NULL)
    return 0;

  os_printf("REST: setup host=%s port=%ld security=%ld\n", rest_host, port, security);

  client->resp_cb = cmd->callback;

  client->host = rest_host;
  client->port = port;
  client->security = security;
  client->ip.addr = 0;

  client->data = (uint8_t*)os_zalloc(1024);

  client->header = (uint8_t*)os_zalloc(4);
  client->header[0] = 0;

  client->content_type = (uint8_t*)os_zalloc(22);
  os_sprintf((char *)client->content_type, "x-www-form-urlencoded");
  client->content_type[21] = 0;

  client->user_agent = (uint8_t*)os_zalloc(9);
  os_sprintf((char *)client->user_agent, "esp-link");

  client->pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
  client->pCon->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));

  client->pCon->type = ESPCONN_TCP;
  client->pCon->state = ESPCONN_NONE;
  client->pCon->proto.tcp->local_port = espconn_port();
  client->pCon->proto.tcp->remote_port = client->port;

  client->pCon->reverse = client;

  return (uint32_t)client;
}

uint32_t ICACHE_FLASH_ATTR
REST_SetHeader(CmdPacket *cmd) {
  CmdRequest req;
  RestClient *client;
  uint16_t len;
  uint32_t header_index, client_ptr = 0;

  CMD_Request(&req, cmd);

  if(CMD_GetArgc(&req) != 3)
    return 0;

  // Get client -- TODO: Whoa, this is totally unsafe!
  if (CMD_PopArg(&req, (uint8_t*)&client_ptr, 4)) return 0;
  client = (RestClient*)client_ptr;

  // Get header selector
  if (CMD_PopArg(&req, (uint8_t*)&header_index, 4)) return 0;

  // Get header value
  len = CMD_ArgLen(&req);
  if (len > 256) return 0; //safety check
  switch(header_index) {
  case HEADER_GENERIC:
    if(client->header)
      os_free(client->header);
    client->header = (uint8_t*)os_zalloc(len + 1);
    CMD_PopArg(&req, (uint8_t*)client->header, len);
    client->header[len] = 0;
    os_printf("REST: Set header: %s\r\n", client->header);
    break;
  case HEADER_CONTENT_TYPE:
    if(client->content_type)
      os_free(client->content_type);
    client->content_type = (uint8_t*)os_zalloc(len + 1);
    CMD_PopArg(&req, (uint8_t*)client->content_type, len);
    client->content_type[len] = 0;
    os_printf("REST: Set content_type: %s\r\n", client->content_type);
    break;
  case HEADER_USER_AGENT:
    if(client->user_agent)
      os_free(client->user_agent);
    client->user_agent = (uint8_t*)os_zalloc(len + 1);
    CMD_PopArg(&req, (uint8_t*)client->user_agent, len);
    client->user_agent[len] = 0;
    os_printf("REST: Set user_agent: %s\r\n", client->user_agent);
    break;
  }
  return 1;
}

uint32_t ICACHE_FLASH_ATTR
REST_Request(CmdPacket *cmd) {
  CmdRequest req;
  RestClient *client;
  uint16_t len, realLen = 0;
  uint32_t client_ptr;
  uint8_t *body = NULL;
  uint8_t method[16];
  uint8_t path[1024];

  CMD_Request(&req, cmd);

  if(CMD_GetArgc(&req) <3)
    return 0;

  // Get client -- TODO: Whoa, this is totally unsafe!
  if(CMD_PopArg(&req, (uint8_t*)&client_ptr, 4)) return 0;
  client = (RestClient*)client_ptr;

  // Get HTTP method
  len = CMD_ArgLen(&req);
  if (len > 15) return 0;
  CMD_PopArg(&req, method, len);
  method[len] = 0;

  // Get HTTP path
  len = CMD_ArgLen(&req);
  if (len > 1023) return 0;
  CMD_PopArg(&req, path, len);
  path[len] = 0;

  // Get HTTP body
  if (CMD_GetArgc(&req) == 3){
    realLen = 0;
    len = 0;
  } else {
    CMD_PopArg(&req, (uint8_t*)&realLen, 4);

    len = CMD_ArgLen(&req);
    if (len > 2048 || realLen > len) return 0;
    body = (uint8_t*)os_zalloc(len + 1);
    CMD_PopArg(&req, body, len);
    body[len] = 0;
  }

  client->pCon->state = ESPCONN_NONE;

  os_printf("REST: method: %s, path: %s\n", method, path);

  client->data_len = os_sprintf((char*)client->data, "%s %s HTTP/1.1\r\n"
                        "Host: %s\r\n"
                        "%s"
                        "Content-Length: %d\r\n"
                        "Connection: close\r\n"
                        "Content-Type: %s\r\n"
                        "User-Agent: %s\r\n\r\n",
                        method, path,
                        client->host,
                        client->header,
                        realLen,
                        client->content_type,
                        client->user_agent);

  if (realLen > 0){
    os_memcpy(client->data + client->data_len, body, realLen);
    client->data_len += realLen;
    //os_sprintf(client->data + client->data_len, "\r\n\r\n");
    //client->data_len += 4;
  }

  client->pCon->state = ESPCONN_NONE;
  espconn_regist_connectcb(client->pCon, tcpclient_connect_cb);
  espconn_regist_reconcb(client->pCon, tcpclient_recon_cb);

  if(UTILS_StrToIP((char *)client->host, &client->pCon->proto.tcp->remote_ip)) {
    os_printf("REST: Connect to ip %s:%ld\n",client->host, client->port);
    //if(client->security){
    //  espconn_secure_connect(client->pCon);
    //}
    //else {
      espconn_connect(client->pCon);
    //}
  } else {
    os_printf("REST: Connect to host %s:%ld\n", client->host, client->port);
    espconn_gethostbyname(client->pCon, (char *)client->host, &client->ip, rest_dns_found);
  }

  if(body) os_free(body);
  return 1;
}

uint8_t ICACHE_FLASH_ATTR
UTILS_StrToIP(const char* str, void *ip)
{
  /* The count of the number of bytes processed. */
  int i;
  /* A pointer to the next digit to process. */
  const char * start;

  start = str;
  for (i = 0; i < 4; i++) {
    /* The digit being processed. */
    char c;
    /* The value of this byte. */
    int n = 0;
    while (1) {
      c = * start;
      start++;
      if (c >= '0' && c <= '9') {
        n *= 10;
        n += c - '0';
      }
      /* We insist on stopping at "." if we are still parsing
         the first, second, or third numbers. If we have reached
         the end of the numbers, we will allow any character. */
      else if ((i < 3 && c == '.') || i == 3) {
        break;
      }
      else {
        return 0;
      }
    }
    if (n >= 256) {
      return 0;
    }
    ((uint8_t*)ip)[i] = n;
  }
  return 1;
}
