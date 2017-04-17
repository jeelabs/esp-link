/*
 * Copyright (c) 2016, 2017 Danny Backx
 *
 * License (MIT license):
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *   THE SOFTWARE.
 *
 * This contains code to "punch a hole" in a NAT firewall, so a device on the inside
 * network can be accessed from the outside by parties that know how to do so.
 *
 * This is basically a poor man's version of some of the UPnP protocols.
 */

#include "esp8266.h"
#include "sntp.h"
#include "cmd.h"
#include <socket.h>
#include <ip_addr.h>
#include <strings.h>

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

/*
 * Query #1 : SSDP protocol, this is a UDP multicast to discover a IGD device.
 * (internet gateway device)
 */
static const char *upnp_ssdp_multicast = "239.255.255.250";
static short upnp_server_port = 1900;
static const char *ssdp_message = "M-SEARCH * HTTP/1.1\r\n"
	"HOST: 239.255.255.250:1900\r\n"
	"ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
	"MAN: \"ssdp:discover\"\r\n"
	"MX: 2\r\n";
static int ssdp_len;
static char *control_url = 0;
static int counter;

/*
 * Query #2 : query the IGD for its device & service list.
 * We search this list for the control URL of the router.
 * The protocol is now UPnP (universal plug and play), HTTP over TCP, usually with SOAP XML
 * encoding. In this particular query, the query itself is a HTTP GET, no XML.
 * But its reply is SOAP XML formatted.
 */
// BTW, use http/1.0 to avoid responses with transfer-encoding: chunked
static const char *upnp_general_query = "GET %s HTTP/1.0\r\n"
		   "Host: %s\r\n"
                   "Connection: close\r\n"
                   "User-Agent: esp-link\r\n\r\n";

/*
 * Subsequent queries aren't necessarily in order
 *
 * This one queries the IGD for its external IP address.
 */
static const char *upnp_query_external_address = "POST %s HTTP/1.0\r\n"	// control-url
	"Host: %s\r\n"							// host ip+port
	"User-Agent: esp-link\r\n"
	"Content-Length : %d\r\n"					// XML length
	"Content-Type: text/xml\r\n"
	"SOAPAction: \"urn:schemas-upnp-org:service:WANPPPConnection:1#GetExternalIPAddress\"\r\n"
	"Connection: Close\r\n"
	"Cache-Control: no-cache\r\n"
	"Pragma: no-cache\r\n"
	"\r\n%s";							// XML

static const char *upnp_query_external_address_xml =
	"<?xml version=\"1.0\"?>\r\n"
	"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
	"<s:Body>\r\n"
	"<u:GetExternalIPAddress xmlns:u=\"urn:schemas-upnp-org:service:WANPPPConnection:1\">\r\n"
	"</u:GetExternalIPAddress>\r\n"
	"</s:Body>\r\n"
	"</s:Envelope>\r\n";
  /*
    Sample query

	POST /o8ee3npj36j/IGD/upnp/control/igd/wanpppc_1_1_1 HTTP/1.1
	Host: 192.168.1.1:8000
	User-Agent: Ubuntu/16.04, UPnP/1.1, MiniUPnPc/2.0
	Content-Length: 286
	Content-Type: text/xml
	SOAPAction: "urn:schemas-upnp-org:service:WANPPPConnection:1#GetExternalIPAddress"
	Connection: Close
	Cache-Control: no-cache
	Pragma: no-cache

	<?xml version="1.0"?>
	<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"
	    s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
	  <s:Body>
	     <u:GetExternalIPAddress xmlns:u="urn:schemas-upnp-org:service:WANPPPConnection:1">
	     </u:GetExternalIPAddress>
	  </s:Body>
	</s:Envelope>

    and its reply
	<?xml version="1.0"?>
	<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"
	    s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
	  <s:Body>
	    <m:GetExternalIPAddressResponse
	        xmlns:m="urn:schemas-upnp-org:service:WANPPPConnection:1">
	      <NewExternalIPAddress>213.49.166.224</NewExternalIPAddress>
	    </m:GetExternalIPAddressResponse>
	  </s:Body>
	</s:Envelope>

  */

static const char *upnp_query_add_tmpl =
	"POST %s HTTP/1.0\r\n"			// Control URL
	"Host: %s\r\n"				// IGD ip+port
	"User-Agent: esp-link\r\n"
	"Content-Length: %d\r\n"		// Length of XML
	"Content-Type: text/xml\r\n"
	"SOAPAction: \"urn:schemas-upnp-org:service:WANPPPConnection:1#AddPortMapping\"\r\n"
	"Connection: Close\r\n"
	"Cache-Control: no-cache\r\n"
	"Pragma: no-cache\r\n\r\n%s";		// XML

static const char *upnp_query_add_xml =
	"<?xml version=\"1.0\"?>\r\n"
	"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
	" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
	"<s:Body>\r\n"
	"<u:AddPortMapping xmlns:u=\"urn:schemas-upnp-org:service:WANPPPConnection:1\">\r\n"
	"<NewRemoteHost></NewRemoteHost>\r\n"
	"<NewExternalPort>%d</NewExternalPort>\r\n"			// remote port
	"<NewProtocol>TCP</NewProtocol>\r\n"
	"<NewInternalPort>%d</NewInternalPort>\r\n"			// local port
	"<NewInternalClient>%d.%d.%d.%d</NewInternalClient>\r\n"	// local ip
	"<NewEnabled>1</NewEnabled>\r\n"
	"<NewPortMappingDescription>libminiupnpc</NewPortMappingDescription>\r\n"
	"<NewLeaseDuration>0</NewLeaseDuration>\r\n"
	"</u:AddPortMapping>\r\n"
	"</s:Body>\r\n"
	"</s:Envelope>\r\n";
/*
 * This query adds a port mapping

	POST /o8ee3npj36j/IGD/upnp/control/igd/wanpppc_1_1_1 HTTP/1.1
	Host: 192.168.1.1:8000
	User-Agent: Ubuntu/16.04, UPnP/1.1, MiniUPnPc/2.0
	Content-Length: 594
	Content-Type: text/xml
	SOAPAction: "urn:schemas-upnp-org:service:WANPPPConnection:1#AddPortMapping"
	Connection: Close
	Cache-Control: no-cache
	Pragma: no-cache

	<?xml version="1.0"?>
	<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"
	    s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
	  <s:Body>
	    <u:AddPortMapping xmlns:u="urn:schemas-upnp-org:service:WANPPPConnection:1">
	      <NewRemoteHost></NewRemoteHost>
	      <NewExternalPort>9876</NewExternalPort>
	      <NewProtocol>TCP</NewProtocol>
	      <NewInternalPort>80</NewInternalPort>
	      <NewInternalClient>192.168.1.176</NewInternalClient>
	      <NewEnabled>1</NewEnabled>
	      <NewPortMappingDescription>libminiupnpc</NewPortMappingDescription>
	      <NewLeaseDuration>0</NewLeaseDuration>
	    </u:AddPortMapping>
	  </s:Body>
	</s:Envelope>

    and the reply

	HTTP/1.0 200 OK
	Connection: close
	Date: Sat, 25 Mar 2017 16:54:41 GMT
	Server: MediaAccess TG 789Ovn Xtream 10.A.0.I UPnP/1.0 (9C-97-26-26-44-DE)
	Content-Length: 300
	Content-Type: text/xml; charset="utf-8"
	EXT:

	<?xml version="1.0"?>
	<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"
	    s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
	  <s:Body>
	<m:AddPortMappingResponse xmlns:m="urn:schemas-upnp-org:service:WANPPPConnection:1">
	</m:AddPortMappingResponse>
	</s:Body>
	</s:Envelope>
 */

 /*
  * Query to delete a port mapping

	POST /o8ee3npj36j/IGD/upnp/control/igd/wanpppc_1_1_1 HTTP/1.1
	Host: 192.168.1.1:8000
	User-Agent: Ubuntu/16.04, UPnP/1.1, MiniUPnPc/2.0
	Content-Length: 380
	Content-Type: text/xml
	SOAPAction: "urn:schemas-upnp-org:service:WANPPPConnection:1#DeletePortMapping"
	Connection: Close
	Cache-Control: no-cache
	Pragma: no-cache

	<?xml version="1.0"?>
	<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"
	    s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
	  <s:Body>
	    <u:DeletePortMapping xmlns:u="urn:schemas-upnp-org:service:WANPPPConnection:1">
	    <NewRemoteHost>
	    </NewRemoteHost>
	    <NewExternalPort>9876</NewExternalPort>
	    <NewProtocol>TCP</NewProtocol>
	  </u:DeletePortMapping>
	  </s:Body>
	</s:Envelope>

    and its reply :

	<?xml version="1.0"?>
	<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"
	    s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
	  <s:Body>
	    <m:DeletePortMappingResponse xmlns:m="urn:schemas-upnp-org:service:WANPPPConnection:1">
	    </m:DeletePortMappingResponse>
	  </s:Body>
	</s:Envelope>
 */

typedef struct {
  char			*host, *path, *location;	// IGD specifics
  uint16_t		control_port;

  uint32_t		port, remote_port;		// local, remote
  ip_addr_t		ip, remote_ip;			// local, remote

  struct espconn	*con;				// comms handler

  char			*data;				// incremental data transmission
  uint16_t		data_len, data_sent;
} UPnPClient;

static UPnPClient *the_client;

// Functions
static void upnp_query_igd(UPnPClient *);
static void ssdp_sent_cb(void *arg);
static void ssdp_recv_cb(void *arg, char *pusrdata, unsigned short length);
static void upnp_tcp_sent_cb(void *arg);
static void upnp_tcp_discon_cb(void *arg);
static void upnp_tcp_recon_cb(void *arg, sint8 errType);
static void upnp_tcp_connect_cb(void *arg);
static void upnp_dns_found(const char *name, ip_addr_t *ipaddr, void *arg);
static void upnp_tcp_recv_cb(void *arg, char *pdata, unsigned short len);
static void upnp_cleanup_conn(UPnPClient *);

static void upnp_analyze_location(UPnPClient *, char *, int);

static void ICACHE_FLASH_ATTR
ssdp_recv_cb(void *arg, char *pusrdata, unsigned short length) {
  struct espconn *con = (struct espconn *)arg;
  UPnPClient *client = con->reverse;

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

	upnp_analyze_location(client, pusrdata+j, k-j);

	upnp_cleanup_conn(client);

	// Trigger next query
	upnp_state = upnp_found_igd;
	upnp_query_igd(client);
	break;
      }
    break;
  default:
    os_printf("UPnP FSM issue, upnp_state = %d\n", (int)upnp_state);
  }
}

// Our packets are small, so this is not useful
static void ICACHE_FLASH_ATTR
ssdp_sent_cb(void *arg) {
  struct espconn *con = (struct espconn *)arg;
  os_printf("ssdp_sent_cb, count %d\n", counter);

#if 1
  if (upnp_state == upnp_multicasted && counter < counter_max) {
    counter++;
    espconn_sent(con, (uint8_t*)ssdp_message, ssdp_len);
  }
#else
  os_printf("Disabled ssdp_sent_cb\n");
#endif
}

/*
 * This should be a generic function that performs a query via TCP.
 */
static void ICACHE_FLASH_ATTR upnp_query_igd(UPnPClient *client) {
  char *query;

  // Create new espconn
  client->con = (struct espconn *)os_zalloc(sizeof(struct espconn));
  client->con->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
  client->con->reverse = client;
  
  // os_printf("upnp_query_igd : new espconn structure %08x\n", (uint32_t)client->con);

  client->con->proto.tcp->local_port = espconn_port();
  client->con->proto.tcp->remote_port = client->control_port;
  client->con->type = ESPCONN_TCP;
  client->con->state = ESPCONN_NONE;

  espconn_regist_connectcb(client->con, upnp_tcp_connect_cb);
  espconn_regist_reconcb(client->con, upnp_tcp_recon_cb);
  espconn_regist_disconcb(client->con, upnp_tcp_discon_cb);
  espconn_regist_recvcb(client->con, upnp_tcp_recv_cb);
  espconn_regist_sentcb(client->con, upnp_tcp_sent_cb);

  switch (upnp_state) {
  case upnp_found_igd:
    /*
     * Start the UPnP query of the IGD so we know its control URL.
     */
    query = (char *)os_malloc(strlen(upnp_general_query) + strlen(client->path) + strlen(client->location));
    os_sprintf(query, upnp_general_query, client->path, client->location);
    break;

  case upnp_adding_port:
    /*
     * Start the query to add a port forwarding definition to the IGD.
     */
    os_printf("Port to be added is %08x : %04x (remote %04x)\n", client->ip.addr, client->port, client->remote_port);

    // Two step approach, as the XML also contains variable info, and we need to pass its length.
    char *xml = (char *)os_malloc(strlen(upnp_query_add_xml) + 32);

    // FIXME byte order issue ?
    os_sprintf(xml, upnp_query_add_xml, client->remote_port, client->port,
      ip4_addr4(&client->ip), ip4_addr3(&client->ip),
      ip4_addr2(&client->ip), ip4_addr1(&client->ip));

    // Step 2 : create the headers.
    query = (char *)os_malloc(strlen(upnp_query_add_tmpl) + strlen(client->path) + strlen(client->host) + 10 + strlen(xml));
    os_sprintf(query, upnp_query_add_tmpl,
      client->path, client->host,
      strlen(xml), xml);

    // Don't forget to free the temporary string storage for step 1.
    os_free(xml);
#if 1
    os_printf("Query : %s\n", query);
    break;
#else
    os_printf("Query : %s\n", query);
    os_free(query);
    return;
#endif
  default:
    os_printf("upnp_query_igd: untreated state %d\n", (int)upnp_state);
    query = "";
    return;
  }

  client->data = query;
  client->data_len = strlen(query);

  if (UTILS_StrToIP(client->host, &client->con->proto.tcp->remote_ip)) {
    memcpy(&client->remote_ip, client->con->proto.tcp->remote_ip, 4);
    ip_addr_t rip = client->remote_ip;

    int r = espconn_connect(client->con);
    os_printf("Connect to %d.%d.%d.%d : %d -> %d\n", ip4_addr1(&rip), ip4_addr2(&rip),
      ip4_addr3(&rip), ip4_addr4(&rip), client->con->proto.tcp->remote_port, r);

  } else {
    // Perform DNS query
    os_printf("UPnP: lookup host %s\n", client->host);
    espconn_gethostbyname(client->con, client->host, (ip_addr_t *)&client->con->proto.tcp->remote_ip[0], upnp_dns_found);

    // Pick up next round of code in upnp_dns_found()
  }
}

static void ICACHE_FLASH_ATTR
upnp_tcp_sent_cb(void *arg) {
  struct espconn *con = (struct espconn *)arg;
  UPnPClient *client = con->reverse;

  os_printf("upnp_tcp_sent_cb, len %d %d\n", client->data_sent, client->data_len);

  if (client->data_sent != client->data_len) {
    // we only sent part of the buffer, send the rest
    espconn_send(client->con, (uint8_t*)(client->data+client->data_sent),
          client->data_len-client->data_sent);
    client->data_sent = client->data_len;
  } else {
    // we're done sending, free the memory
    if (client->data) os_free(client->data);
    client->data = 0;
  }
}

static void ICACHE_FLASH_ATTR
upnp_tcp_discon_cb(void *arg) {
  struct espconn *con = (struct espconn *)arg;
  UPnPClient *client = con->reverse;

#if 0
  os_printf("upnp_tcp_discon_cb (empty)\n");

#else
  os_printf("upnp_tcp_discon_cb\n");

  // free the data buffer, if we have one
  if (client->data) os_free(client->data);
  client->data = 0;

  // Free TCP structure
  if (con->proto.tcp) os_free(con->proto.tcp);
  con->proto.tcp = 0;

  espconn_delete(con);
  os_free(con);
  client->con = 0;

  // Kick SM into next state, trigger next query
  switch (upnp_state) {
  case upnp_found_igd:
    upnp_state = upnp_ready;

#if 0
    // FIXME test code
    os_printf("Kick SM into new state\n");
    upnp_state = upnp_adding_port;
    upnp_query_igd(client);
#endif
    break;
  default:
    os_printf("upnp_tcp_discon_cb upnp_state %d\n", upnp_state);
  }
#endif
}

static void ICACHE_FLASH_ATTR
upnp_tcp_recon_cb(void *arg, sint8 errType) {
#if 1
  os_printf("upnp_tcp_recon_cb (empty)\n");
  // struct espconn *pCon = (struct espconn *)arg;

#else
  os_printf("REST #%d: conn reset, err=%d\n", client-restClient, errType);
  // free the data buffer, if we have one
  if (client->data) os_free(client->data);
  client->data = 0;
#endif
}

static void ICACHE_FLASH_ATTR
upnp_tcp_connect_cb(void *arg) {
#if 0
  os_printf("upnp_tcp_connect_cb (empty)\n");
#else
  os_printf("upnp_tcp_connect_cb\n");

  struct espconn *con = (struct espconn *)arg;
  UPnPClient *client = (UPnPClient *)con->reverse;

  espconn_regist_disconcb(con, upnp_tcp_discon_cb);
  espconn_regist_recvcb(con, upnp_tcp_recv_cb);
  espconn_regist_sentcb(con, upnp_tcp_sent_cb);

  client->data_sent = client->data_len <= 1400 ? client->data_len : 1400;

  // os_printf("UPnP sending %d {%s}\n", client->data_sent, client->data);
  os_printf("UPnP sending %d\n", client->data_sent);

  espconn_send(con, (uint8_t*)client->data, client->data_sent);
#endif
}

static void ICACHE_FLASH_ATTR
upnp_dns_found(const char *name, ip_addr_t *ipaddr, void *arg) {
  struct espconn *con = (struct espconn *)arg;
  UPnPClient* client = (UPnPClient *)con->reverse;

  if (ipaddr == NULL) {
    os_printf("REST DNS: Got no ip, try to reconnect\n");
    return;
  }
  os_printf("REST DNS: found ip %d.%d.%d.%d\n",
      *((uint8 *) &ipaddr->addr),
      *((uint8 *) &ipaddr->addr + 1),
      *((uint8 *) &ipaddr->addr + 2),
      *((uint8 *) &ipaddr->addr + 3));
  if (client && client->remote_ip.addr == 0 && ipaddr->addr != 0) {
    os_memcpy(client->con->proto.tcp->remote_ip, &ipaddr->addr, 4);

#ifdef CLIENT_SSL_ENABLE
    if(client->security) {
      espconn_secure_connect(client->con);
    } else
#endif
    espconn_connect(client->con);
    os_printf("REST: connecting...\n");
  }
}

/*
 * FIXME this should buffer the input.
 * Current implementation breaks if message is cut by TCP packets in unexpected places.
 * E.g. packet 1 ends with "<devi" and packet 2 starts with "ce>".
 */
static void ICACHE_FLASH_ATTR
upnp_tcp_recv_cb(void *arg, char *pdata, unsigned short len) {
  // struct espconn *con = (struct espconn*)arg;
  // UPnPClient *client = (UPnPClient *)con->reverse;

  int inservice = 0, get_this = -1;

  os_printf("upnp_tcp_recv_cb len %d\n", len);

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

	// upnp_state = upnp_ready;	// upnp_tcp_discon_cb will do this
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
    os_printf("upnp_state (not treated) %d\n", (int)upnp_state);
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
  os_printf("cmdUPnPScan()\n");

  if (upnp_state == upnp_ready) {
    // Return the IP address of the gateway, this indicates success.
    if (the_client == 0)
      cmdResponseStart(CMD_RESP_V, 0, 0);
    else
      cmdResponseStart(CMD_RESP_V, (uint32_t)the_client->remote_ip.addr, 0);
    cmdResponseEnd();
    return;
  }

  upnp_state = upnp_none;

  struct espconn *con = (struct espconn *)os_zalloc(sizeof(struct espconn));
  if (con == NULL) {
    os_printf("SOCKET : Setup failed to alloc memory for client_pCon\n");

    // Return 0, this means failure
    cmdResponseStart(CMD_RESP_V, 0, 0);
    cmdResponseEnd();

    return;
  }
  counter = 0;

  con->type = ESPCONN_UDP;
  con->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
  if (con->proto.udp == NULL) {
    os_printf("SOCKET : Setup failed to alloc memory for client->pCon->proto.udp\n");

    // Return 0, this means failure
    cmdResponseStart(CMD_RESP_V, 0, 0);
    cmdResponseEnd();

    return;
  }

  UPnPClient *client = (UPnPClient *)os_zalloc(sizeof(UPnPClient));
  client->con = con;
  con->reverse = client;
  the_client = client;

  con->state = ESPCONN_NONE;

  con->proto.udp->remote_port = upnp_server_port;
  con->proto.udp->local_port = espconn_port();

  espconn_regist_sentcb(con, ssdp_sent_cb);
  espconn_regist_recvcb(con, ssdp_recv_cb);

  os_printf("SOCKET : Create connection to ip %s:%d\n", upnp_ssdp_multicast, upnp_server_port);

  if (UTILS_StrToIP((char *)upnp_ssdp_multicast, &con->proto.udp->remote_ip)) {
    espconn_create(con);
  } else {
    os_printf("SOCKET : failed to copy remote_ip to &con->proto.udp->remote_ip\n");

    // Return 0, this means failure
    cmdResponseStart(CMD_RESP_V, 0, 0);
    cmdResponseEnd();

    return;
  }

  os_printf("Determining strlen(ssdp_message)\n");
  ssdp_len = strlen(ssdp_message);
  os_printf("strlen(ssdp_message) = %d\n", ssdp_len);
  espconn_sent(con, (uint8_t*)ssdp_message, ssdp_len);
  os_printf("espconn_sent() done\n");

  // DBG_UPNP("SOCKET : sending %d bytes\n", ssdp_len);
  upnp_state = upnp_multicasted;

  // Not ready yet --> indicate failure
  cmdResponseStart(CMD_RESP_V, 0, 0);
  cmdResponseEnd();

  os_printf("Return at end of cmdUPnPScan(), upnp_state = upnp_multicasted\n");
}

void ICACHE_FLASH_ATTR
cmdUPnPAddPort(CmdPacket *cmd) {
  CmdRequest	req;
  uint32_t	ip;
  uint16_t	local_port, remote_port;

  // start parsing the command
  cmdRequest(&req, cmd);
  if(cmdGetArgc(&req) != 3) {
    os_printf("UPnPAddPort parse command failure: (cmdGetArgc(&req) != 3)\n");
    return;
  }

  // Get the IP address
  if (cmdPopArg(&req, (uint8_t*)&ip, 4)) {
    os_printf("UPnPAddPort parse command failure: cannot get port\n");
    return;
  }
  // get the ports
  if (cmdPopArg(&req, (uint8_t*)&local_port, 2)) {
    os_printf("UPnPAddPort parse command failure: cannot get port\n");
    return;
  }
  if (cmdPopArg(&req, (uint8_t*)&remote_port, 2)) {
    os_printf("UPnPAddPort parse command failure: cannot get port\n");
    return;
  }

  os_printf("UPnPAddPort %08x %04x %04x\n", ip, local_port, remote_port);

  // Return 0
  cmdResponseStart(CMD_RESP_V, 0, 0);
  cmdResponseEnd();
}

#if 0
// Currently this is test code
void ICACHE_FLASH_ATTR
cmdUPnPAddPort(CmdPacket *cmd) {
  UPnPClient *client = (UPnPClient *)os_zalloc(sizeof(UPnPClient));
  os_memset(client, 0, sizeof(UPnPClient));

  char *lp = "http://192.168.1.1:8000/o8ee3npj36j/IGD/upnp/IGD.xml";
  upnp_analyze_location(client, lp, strlen(lp));

  client->con = (struct espconn *)os_zalloc(sizeof(struct espconn));
  client->con->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
  client->con->reverse = client;

  // os_printf("UPnP test : new espconn structure %08x\n", (uint32_t)client->con);

  client->con->proto.tcp->local_port = espconn_port();
  client->con->proto.tcp->remote_port = client->control_port;
  client->con->type = ESPCONN_TCP;
  client->con->state = ESPCONN_NONE;

  char *query = (char *)os_malloc(strlen(upnp_general_query) + strlen(client->path) + strlen(client->location));
  os_sprintf(query, upnp_general_query, client->path, client->location);
  client->data = query;
  client->data_len = strlen(query);

  espconn_regist_connectcb(client->con, upnp_tcp_connect_cb);
  espconn_regist_reconcb(client->con, upnp_tcp_recon_cb);
  espconn_regist_disconcb(client->con, upnp_tcp_discon_cb);
  espconn_regist_recvcb(client->con, upnp_tcp_recv_cb);
  espconn_regist_sentcb(client->con, upnp_tcp_sent_cb);

  // Get recv_cb to start sending stuff
  upnp_state = upnp_found_igd;

  // Set up stuff for "add port"
  client->ip.addr = 0xC0A80196;
  client->port = 0x1234;
  client->remote_port = 0x5678;

  if (UTILS_StrToIP(client->host, &client->con->proto.tcp->remote_ip)) {
    memcpy(&client->remote_ip, client->con->proto.tcp->remote_ip, 4);
    ip_addr_t rip = client->remote_ip;

    int r = espconn_connect(client->con);
    os_printf("Connect to %d.%d.%d.%d : %d -> %d\n", ip4_addr1(&rip), ip4_addr2(&rip),
      ip4_addr3(&rip), ip4_addr4(&rip), client->con->proto.tcp->remote_port, r);

  } else {
    // Perform DNS query
    os_printf("UPnP: lookup host %s\n", client->host);
    espconn_gethostbyname(client->con, client->host, (ip_addr_t *)&client->con->proto.tcp->remote_ip[0], upnp_dns_found);

    // Pick up next round of code in upnp_dns_found()
  }
}
#endif

void ICACHE_FLASH_ATTR
cmdUPnPRemovePort(CmdPacket *cmd) {
}

void ICACHE_FLASH_ATTR
cmdUPnPBegin(CmdPacket *cmd) {
  upnp_state = upnp_none;
}

/*
 * Free/disconnect old structure
 */
static void ICACHE_FLASH_ATTR upnp_cleanup_conn(UPnPClient *client) {
  espconn_delete(client->con);
  os_free(client->con);
  client->con = 0;
}

static void ICACHE_FLASH_ATTR
upnp_analyze_location(UPnPClient *client, char *orig_loc, int len) {
  // Copy location
  client->location = os_malloc(len+1);
  strncpy(client->location, orig_loc, len);
  client->location[len] = 0;

  // Analyse LOCATION
  int i, p=0, q=0;
  for (i=7; client->location[i]; i++)
    if (client->location[i] == ':') {
      p = i+1;
      break;
    }
  if (p != 0)
    client->control_port = atoi(client->location+p);
  else
    client->control_port = 80;
  
  // Continue doing so : now the path
  for (; client->location[i] && client->location[i] != '/'; i++) ;
  if (client->location[i] == '/') {
    client->path = client->location + i;
    q = i;
  } else
    client->path = "";	// FIX ME not sure what to do if no path

  // Cut of the host at the start of path
  if (q) {
    client->host = os_malloc(q-6);
    strncpy(client->host, client->location+7, q-7);
    client->host[q-7] = 0;
  } else {
    client->host = os_malloc(strlen(client->location+7)+1);
    strcpy(client->host, client->location+7);
  }

  os_printf("upnp_analyze_location : location {%s} port %d\n", client->location, client->control_port);
  os_printf("path {%s} host {%s}\n", client->path, client->host);

}

void ICACHE_FLASH_ATTR
cmdUPnPQueryExternalAddress(CmdPacket *cmd) {
  char *query = os_malloc(strlen(upnp_query_external_address) + strlen(the_client->path) +
    strlen(the_client->host) + 8 + strlen(upnp_query_external_address_xml));
  os_sprintf(query, upnp_query_external_address,
    the_client->path, the_client->host,
    strlen(upnp_query_external_address_xml), upnp_query_external_address_xml);
}
