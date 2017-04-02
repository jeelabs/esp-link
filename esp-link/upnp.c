#include "esp8266.h"
#include "sntp.h"
#include "cmd.h"
#include <socket.h>
#include <ip_addr.h>

#if 1
#define DBG_SOCK(format, ...) os_printf(format, ## __VA_ARGS__)
#else
#define DBG_SOCK(format, ...) do { } while(0)
#endif

#define location_size	80
static const int counter_max = 4;

enum upnp_state_t {
	upnp_none,
	upnp_multicasted,
	upnp_found_igd
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
static int counter;

static void ICACHE_FLASH_ATTR
upnp_recv_cb(void *arg, char *pusrdata, unsigned short length) {
  // struct espconn *pCon = (struct espconn *)arg;

  os_printf("upnp_recv_cb : %d bytes\n", length);

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
        DBG_SOCK("len %d message %s\n", len, pusrdata+j);

	// Trigger next query
	break;
      }
    break;
  default:
    os_printf("UPnP FSM issue\n");
  }
}

// Our packets are small, so this is not useful
static void ICACHE_FLASH_ATTR
upnp_sent_cb(void *arg) {
  struct espconn *con = (struct espconn *)arg;
  os_printf("upnp_sent_cb\n");

  if (counter < counter_max) {
    counter++;
    espconn_sent(con, (uint8_t*)ssdp_message, ssdp_len);
  }
}

void ICACHE_FLASH_ATTR
cmdUPnPScan(CmdPacket *cmd) {
  upnp_state = upnp_none;

  os_printf("cmdUPnPScan()\n");
  // cmdResponseStart(CMD_RESP_V, wifiState, 0);
  // cmdResponseEnd();

  struct espconn *con = (struct espconn *)os_zalloc(sizeof(struct espconn));
  if (con == NULL) {
    DBG_SOCK("SOCKET : Setup failed to alloc memory for client_pCon\n");
    return;
  }
  counter = 0;

  con->type = ESPCONN_UDP;
  con->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
  if (con->proto.udp == NULL) {
    DBG_SOCK("SOCKET : Setup failed to alloc memory for client->pCon->proto.udp\n");
    return;
  }

  con->state = ESPCONN_NONE;

  con->proto.udp->remote_port = upnp_server_port;
  con->proto.udp->local_port = upnp_local_port;

  espconn_regist_sentcb(con, upnp_sent_cb);
  espconn_regist_recvcb(con, upnp_recv_cb);

  DBG_SOCK("SOCKET : Create connection to ip %s:%d\n", upnp_ssdp_multicast, upnp_server_port);

  if (UTILS_StrToIP((char *)upnp_ssdp_multicast, &con->proto.udp->remote_ip)) {
    espconn_create(con);
  } else {
    DBG_SOCK("SOCKET : failed to copy remote_ip to &con->proto.udp->remote_ip\n");
    return;
  }

  ssdp_len = strlen(ssdp_message);
  espconn_sent(con, (uint8_t*)ssdp_message, ssdp_len);
  DBG_SOCK("SOCKET : sending %d bytes\n", ssdp_len);
  upnp_state = upnp_multicasted;
}

