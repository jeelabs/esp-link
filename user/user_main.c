#include <esp8266.h>
#include "config.h"
#include "syslog.h"
#include "ems.h"

#define APPINIT_DBG
#ifdef APPINIT_DBG
#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)
#else
#define DBG(format, ...) do { } while(0)
#endif

static struct espconn *pespconn = NULL;
static ip_addr_t addr;
static char host[32];

/******************************************************************************
 *
 ******************************************************************************/
static void ICACHE_FLASH_ATTR appinit_gethostbyname_cb(const char *name, ip_addr_t *ipaddr, void *arg)
{
    os_free(((struct espconn *)arg)->proto.udp);
    os_free((struct espconn *)arg);

    if (ipaddr != NULL) {
	emsInit();
	EMSBusStatus |= EMSBUS_RDY;
	DBG("app_init: EMSBus ready...\n");
    } else {
	pespconn = (espconn *)os_zalloc(sizeof(espconn));
	pespconn->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));

	espconn_gethostbyname(pespconn, host, &addr, appinit_gethostbyname_cb);
	DBG("app_init: retry to resolve %s...\n", host);
    }
}

// initialize the custom stuff that goes beyond esp-link
// we simply try to resolve <hostname.local>
void app_init() {

    if (pespconn == NULL)
      pespconn = (espconn *)os_zalloc(sizeof(espconn));

    if (pespconn->proto.udp == NULL)
      pespconn->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));

    os_sprintf(host, "%s", flashConfig.hostname);
    DBG("app_init: resolving %s...\n", host);

    espconn_gethostbyname(pespconn, host, &addr, appinit_gethostbyname_cb);
}
