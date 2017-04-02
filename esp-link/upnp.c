#include "esp8266.h"
#include "sntp.h"
#include "cmd.h"
#include <socket.h>
#include <ip_addr.h>

#if 1
#define DBG_UPNP(format, ...) os_printf(format, ## __VA_ARGS__)
#else
#define DBG_UPNP(format, ...) do { } while(0)
#endif

#define location_size	80
static const int counter_max = 4;

enum upnp_state_t {
	upnp_none,
	upnp_multicasted,
	upnp_found_igd,
	upnp_ready,
	upnp_adding_port,
	upnp_removing_port
};
static enum upnp_state_t upnp_state;


static const char *upnp_ssdp_multicast = "239.255.255.250";
static short upnp_server_port = 1900;
static short upnp_local_port = -1;
static const char *ssdp_message = "M-SEARCH * HTTP/1.1\r\n"
	"HOST: 239.255.255.250:1900\r\n"
	"ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
	"MAN: \"ssdp:discover\"\r\n"
	"MX: 2\r\n";
static int ssdp_len;
static char location[location_size];
static char *control_url = 0;
static int counter;

typedef struct {
  char			*host;
  char			*path;
  uint32_t		port;
  ip_addr_t		ip;
  struct espconn	*con;
  char			*data;
  uint16_t		data_len;
  uint16_t		data_sent;
} UPnPClient;

// Functions
static void upnp_query_igd();
static void ssdp_sent_cb(void *arg);
static void ssdp_recv_cb(void *arg, char *pusrdata, unsigned short length);
static void upnp_tcp_sent_cb(void *arg);
static void upnp_tcp_discon_cb(void *arg);
static void upnp_tcp_recon_cb(void *arg, sint8 errType);
static void upnp_tcp_connect_cb(void *arg);
static void upnp_dns_found(const char *name, ip_addr_t *ipaddr, void *arg);
static void upnp_tcp_recv(void *arg, char *pdata, unsigned short len);

static void ICACHE_FLASH_ATTR
ssdp_recv_cb(void *arg, char *pusrdata, unsigned short length) {
  // struct espconn *con = (struct espconn *)arg;

  os_printf("ssdp_recv_cb : %d bytes\n", length);

  switch (upnp_state) {
  case upnp_multicasted:
    // Find the LOCATION: field
    for (int i=0; i<length-20; i++)
      if (pusrdata[i] == 0x0D && pusrdata[i+1] == 0x0A
       && os_strncmp(pusrdata+i+2, "LOCATION:", 9) == 0) {
        // find end of LOCATION field
        int j, k;
        for (k=j=i+11; pusrdata[k] && pusrdata[k] != 0x0D; k++) ;
        int len = k-j+1;

        pusrdata[k] = 0; // NULL terminate

	// FIXME this should be dynamically allocated
        os_strncpy(location, pusrdata+j, len);
        DBG_UPNP("len %d message %s\n", len, pusrdata+j);

	// Trigger next query
	upnp_query_igd();
	break;
      }
    break;
  default:
    os_printf("UPnP FSM issue\n");
  }
}

// Our packets are small, so this is not useful
static void ICACHE_FLASH_ATTR
ssdp_sent_cb(void *arg) {
  struct espconn *con = (struct espconn *)arg;
  os_printf("ssdp_sent_cb\n");

  if (upnp_state == upnp_multicasted && counter < counter_max) {
    counter++;
    espconn_sent(con, (uint8_t*)ssdp_message, ssdp_len);
  }
}

static void ICACHE_FLASH_ATTR upnp_query_igd(char *query) {
  struct espconn *con = (struct espconn *)os_zalloc(sizeof(struct espconn));

  UPnPClient *client = (UPnPClient *)os_zalloc(sizeof(UPnPClient));
  client->con = con;
  con->reverse = client;

  con->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
  upnp_state = upnp_found_igd;

  // Analyse LOCATION
  int i, p=0, q=0;
  for (i=7; location[i]; i++)
    if (location[i] == ':') {
      p = i+1;
      break;
    }
  if (p != 0)
    con->proto.tcp->remote_port = atoi(location+p);
  else
    con->proto.tcp->remote_port = 80;
  
  // Continue doing so : now the path
  for (i=7; location[i] && location[i] != '/'; i++) ;
  if (location[i] == '/') {
    client->path = location + i;
    q = i;
  } else
    client->path = "";	// FIX ME not sure what to do if no path
  // Now the smallest of p and q points to end of IP address
  if (p != 0) {
    location[p] = 0;
  } else if (q != 0) {
    location[q] = 0;
  } else { // take the whole string
  }
  char *host = location + 7;

  con->type = ESPCONN_TCP;
  con->state = ESPCONN_NONE;
  con->proto.tcp->local_port = espconn_port();

  client->data = query;
  client->data_len = strlen(query);

  con->state = ESPCONN_NONE;
  espconn_regist_connectcb(con, upnp_tcp_connect_cb);
  espconn_regist_reconcb(con, upnp_tcp_recon_cb);

  if (UTILS_StrToIP(host, &con->proto.tcp->remote_ip)) {
    DBG_UPNP("UPnP: Connect to ip %s:%d\n", host, con->proto.tcp->remote_port);
    espconn_connect(con);
  } else {
    DBG_UPNP("UPnP: Connect to host %s:%d\n", host, con->proto.tcp->remote_port);
    espconn_gethostbyname(con, host, (ip_addr_t *)&con->proto.tcp->remote_ip[0], upnp_dns_found);
  }
}

static void ICACHE_FLASH_ATTR
upnp_tcp_sent_cb(void *arg) {
  // struct espconn *pCon = (struct espconn *)arg;

  DBG_UPNP("UPNP: tcp_sent\n");
#if 0
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
#endif
}

static void ICACHE_FLASH_ATTR
upnp_tcp_discon_cb(void *arg) {
  // struct espconn *pespconn = (struct espconn *)arg;

#if 0
  // free the data buffer, if we have one
  if (client->data) os_free(client->data);
  client->data = 0;
#endif
}

static void ICACHE_FLASH_ATTR
upnp_tcp_recon_cb(void *arg, sint8 errType) {
  // struct espconn *pCon = (struct espconn *)arg;

#if 0
  os_printf("REST #%d: conn reset, err=%d\n", client-restClient, errType);
  // free the data buffer, if we have one
  if (client->data) os_free(client->data);
  client->data = 0;
#endif
}

static void ICACHE_FLASH_ATTR
upnp_tcp_connect_cb(void *arg) {
  struct espconn *con = (struct espconn *)arg;
  UPnPClient* client = (UPnPClient *)con->reverse;

  DBG_UPNP("UPnP : connected\n");
  espconn_regist_disconcb(con, upnp_tcp_discon_cb);
  espconn_regist_recvcb(con, upnp_tcp_recv);
  espconn_regist_sentcb(con, upnp_tcp_sent_cb);

  client->data_sent = client->data_len <= 1400 ? client->data_len : 1400;
  DBG_UPNP("UPnP sending %d\n", client->data_sent);

  espconn_sent(con, (uint8_t*)client->data, client->data_sent);
}

static void ICACHE_FLASH_ATTR
upnp_dns_found(const char *name, ip_addr_t *ipaddr, void *arg) {
  struct espconn *con = (struct espconn *)arg;
  UPnPClient* client = (UPnPClient *)con->reverse;

  if(ipaddr == NULL) {
    os_printf("REST DNS: Got no ip, try to reconnect\n");
    return;
  }
  DBG_UPNP("REST DNS: found ip %d.%d.%d.%d\n",
      *((uint8 *) &ipaddr->addr),
      *((uint8 *) &ipaddr->addr + 1),
      *((uint8 *) &ipaddr->addr + 2),
      *((uint8 *) &ipaddr->addr + 3));
  if(client->ip.addr == 0 && ipaddr->addr != 0) {
    os_memcpy(client->con->proto.tcp->remote_ip, &ipaddr->addr, 4);

#ifdef CLIENT_SSL_ENABLE
    if(client->security) {
      espconn_secure_connect(client->con);
    } else
#endif
    espconn_connect(client->con);
    DBG_UPNP("REST: connecting...\n");
  }
}

/*
 * FIXME this should buffer the input.
 * Current implementation breaks if message is cut by TCP packets in unexpected places.
 * E.g. packet 1 ends with "<devi" and packet 2 starts with "ce>".
 */
static void ICACHE_FLASH_ATTR
upnp_tcp_recv(void *arg, char *pdata, unsigned short len) {
  // struct espconn *con = (struct espconn*)arg;
  // UPnPClient *client = (UPnPClient *)con->reverse;

  int inservice = 0, get_this = -1;

  // os_printf("UPnP TCP Recv len %d\n", len);

  switch (upnp_state) {
  case upnp_found_igd:
    // Find a service with specific id, remember its control-url.
    for (int i=0; i<len; i++) {
      if (strncasecmp(pdata+i, "<service>", 9) == 0) {
	inservice++;
      } else if (strncasecmp(pdata+i, "</service>", 10) == 0) {
	inservice--;
      } else if (strncasecmp(pdata+i, "urn:upnp-org:serviceId:WANPPPConn1", 34) == 0) {
	get_this = inservice;
      } else if (get_this == inservice && strncasecmp(pdata+i, "<controlURL>", 12) == 0) {
	get_this = -1;
	int j;
	for (j=i+12; pdata[j] && pdata[j] != '<'; j++) ;
	int len = j-i-8;
	control_url = os_malloc(len);
	int k=0;
	for (j=i+12; pdata[j] && pdata[j] != '<'; j++, k++)
	  control_url[k] = pdata[j];
	control_url[k] = 0;
	os_printf("UPnP: Control URL %s\n", control_url);

	upnp_state = upnp_ready;
      }
    }
    break;
  case upnp_ready:
    break;
  case upnp_adding_port:
    os_printf("UPnP <adding port> TCP Recv len %d, %s\n", len, pdata);
    break;
  case upnp_removing_port:
    os_printf("UPnP <removing port> TCP Recv len %d, %s\n", len, pdata);
    break;
  default:
    break;
  }
}

/*
 * This triggers the initial conversation to find and query the IGD.
 * Protocol used is SSDP, a part of the UPnP suite.
 * This is UDP based traffic, the initial query is a multicast.
 *
 * Followup is in ssdp_recv_cb().
 */
void ICACHE_FLASH_ATTR
cmdUPnPScan(CmdPacket *cmd) {
  upnp_state = upnp_none;

  os_printf("cmdUPnPScan()\n");

  struct espconn *con = (struct espconn *)os_zalloc(sizeof(struct espconn));
  if (con == NULL) {
    DBG_UPNP("SOCKET : Setup failed to alloc memory for client_pCon\n");
    return;
  }
  counter = 0;

  con->type = ESPCONN_UDP;
  con->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
  if (con->proto.udp == NULL) {
    DBG_UPNP("SOCKET : Setup failed to alloc memory for client->pCon->proto.udp\n");
    return;
  }

  con->state = ESPCONN_NONE;

  con->proto.udp->remote_port = upnp_server_port;
  con->proto.udp->local_port = espconn_port();
  con->reverse = 0;

  espconn_regist_sentcb(con, ssdp_sent_cb);
  espconn_regist_recvcb(con, ssdp_recv_cb);

  DBG_UPNP("SOCKET : Create connection to ip %s:%d\n", upnp_ssdp_multicast, upnp_server_port);

  if (UTILS_StrToIP((char *)upnp_ssdp_multicast, &con->proto.udp->remote_ip)) {
    espconn_create(con);
  } else {
    DBG_UPNP("SOCKET : failed to copy remote_ip to &con->proto.udp->remote_ip\n");
    return;
  }

  ssdp_len = strlen(ssdp_message);
  espconn_sent(con, (uint8_t*)ssdp_message, ssdp_len);
  DBG_UPNP("SOCKET : sending %d bytes\n", ssdp_len);
  upnp_state = upnp_multicasted;
}

// BTW, use http/1.0 to avoid responses with transfer-encoding: chunked
const char *tmpl = "GET %s HTTP/1.0\r\n"
		   "Host: %s\r\n"
                   "Connection: close\r\n"
                   "User-Agent: esp-link\r\n\r\n";

void ICACHE_FLASH_ATTR
cmdUPnPAddPort(CmdPacket *cmd) {
  strcpy(location, "http://192.168.1.1:8000/o8ee3npj36j/IGD/upnp/IGD.xml");
  char *query = (char *)os_malloc(strlen(tmpl) + location_size);
  os_sprintf(query, tmpl, "/o8ee3npj36j/IGD/upnp/IGD.xml", "http://192.168.1.1:8000");
  upnp_query_igd(query);
}

void ICACHE_FLASH_ATTR
cmdUPnPRemovePort(CmdPacket *cmd) {
}
