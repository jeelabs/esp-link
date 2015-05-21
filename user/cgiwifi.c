/*
Cgi/template routines for the /wifi url.
*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain
 * this notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 */


#include <esp8266.h>
#include "cgiwifi.h"
#include "status.h"

//Enable this to disallow any changes in AP settings
//#define DEMO_MODE

// ===== wifi status change callback

uint8_t wifiState = wifiIsDisconnected;
// reasons for which a connection failed
uint8_t wifiReason = 0;
static char *wifiReasons[] = {
	"", "unspecified", "auth_expire", "auth_leave", "assoc_expire", "assoc_toomany", "not_authed",
	"not_assoced", "assoc_leave", "assoc_not_authed", "disassoc_pwrcap_bad", "disassoc_supchan_bad",
	"ie_invalid", "mic_failure", "4way_handshake_timeout", "group_key_update_timeout",
	"ie_in_4way_differs", "group_cipher_invalid", "pairwise_cipher_invalid", "akmp_invalid",
	"unsupp_rsn_ie_version", "invalid_rsn_ie_cap", "802_1x_auth_failed", "cipher_suite_rejected",
	"beacon_timeout", "no_ap_found" };

static char* ICACHE_FLASH_ATTR wifiGetReason(void) {
	if (wifiReason <= 24) return wifiReasons[wifiReason];
	if (wifiReason >= 200 && wifiReason <= 201) return wifiReasons[wifiReason-200+25];
	return wifiReasons[1];
}

// handler for wifi status change callback coming in from espressif library
static void ICACHE_FLASH_ATTR wifiHandleEventCb(System_Event_t *evt) {
	switch (evt->event) {
	case EVENT_STAMODE_CONNECTED:
		wifiState = wifiIsConnected;
		wifiReason = 0;
		os_printf("Wifi connected to ssid %s, ch %d\n", evt->event_info.connected.ssid,
				evt->event_info.connected.channel);
		statusWifiUpdate(wifiState);
		break;
	case EVENT_STAMODE_DISCONNECTED:
		wifiState = wifiIsDisconnected;
		wifiReason = evt->event_info.disconnected.reason;
		os_printf("Wifi disconnected from ssid %s, reason %s\n",
				evt->event_info.disconnected.ssid, wifiGetReason());
		statusWifiUpdate(wifiState);
		break;
	case EVENT_STAMODE_AUTHMODE_CHANGE:
		os_printf("Wifi auth mode: %d -> %d\n",
				evt->event_info.auth_change.old_mode, evt->event_info.auth_change.new_mode);
		break;
	case EVENT_STAMODE_GOT_IP:
		wifiState = wifiGotIP;
		wifiReason = 0;
		os_printf("Wifi got ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR "\n",
				IP2STR(&evt->event_info.got_ip.ip), IP2STR(&evt->event_info.got_ip.mask),
				IP2STR(&evt->event_info.got_ip.gw));
		statusWifiUpdate(wifiState);
		break;
	case EVENT_SOFTAPMODE_STACONNECTED:
		os_printf("Wifi AP: station " MACSTR " joined, AID = %d\n",
				MAC2STR(evt->event_info.sta_connected.mac), evt->event_info.sta_connected.aid);
		break;
	case EVENT_SOFTAPMODE_STADISCONNECTED:
		os_printf("Wifi AP: station " MACSTR " left, AID = %d\n",
				MAC2STR(evt->event_info.sta_disconnected.mac), evt->event_info.sta_disconnected.aid);
		break;
	default:
		break;
	}
}

// ===== wifi scanning

//WiFi access point data
typedef struct {
	char ssid[32];
	char rssi;
	char enc;
} ApData;

//Scan result
typedef struct {
	char scanInProgress; //if 1, don't access the underlying stuff from the webpage.
	ApData **apData;
	int noAps;
} ScanResultData;

//Static scan status storage.
static ScanResultData cgiWifiAps;

//Callback the code calls when a wlan ap scan is done. Basically stores the result in
//the cgiWifiAps struct.
void ICACHE_FLASH_ATTR wifiScanDoneCb(void *arg, STATUS status) {
	int n;
	struct bss_info *bss_link = (struct bss_info *)arg;

	if (status!=OK) {
		os_printf("wifiScanDoneCb status=%d\n", status);
		cgiWifiAps.scanInProgress=0;
		return;
	}

	//Clear prev ap data if needed.
	if (cgiWifiAps.apData!=NULL) {
		for (n=0; n<cgiWifiAps.noAps; n++) os_free(cgiWifiAps.apData[n]);
		os_free(cgiWifiAps.apData);
	}

	//Count amount of access points found.
	n=0;
	while (bss_link != NULL) {
		bss_link = bss_link->next.stqe_next;
		n++;
	}
	//Allocate memory for access point data
	cgiWifiAps.apData=(ApData **)os_malloc(sizeof(ApData *)*n);
	cgiWifiAps.noAps=n;
	os_printf("Scan done: found %d APs\n", n);

	//Copy access point data to the static struct
	n=0;
	bss_link = (struct bss_info *)arg;
	while (bss_link != NULL) {
		if (n>=cgiWifiAps.noAps) {
			//This means the bss_link changed under our nose. Shouldn't happen!
			//Break because otherwise we will write in unallocated memory.
			os_printf("Huh? I have more than the allocated %d aps!\n", cgiWifiAps.noAps);
			break;
		}
		//Save the ap data.
		cgiWifiAps.apData[n]=(ApData *)os_malloc(sizeof(ApData));
		cgiWifiAps.apData[n]->rssi=bss_link->rssi;
		cgiWifiAps.apData[n]->enc=bss_link->authmode;
		strncpy(cgiWifiAps.apData[n]->ssid, (char*)bss_link->ssid, 32);

		bss_link = bss_link->next.stqe_next;
		n++;
	}
	//We're done.
	cgiWifiAps.scanInProgress=0;
}

//Routine to start a WiFi access point scan.
static void ICACHE_FLASH_ATTR wifiStartScan() {
//	int x;
	if (cgiWifiAps.scanInProgress) return;
	cgiWifiAps.scanInProgress=1;
	wifi_station_scan(NULL, wifiScanDoneCb);
}

//This CGI is called from the bit of AJAX-code in wifi.tpl. It will initiate a
//scan for access points and if available will return the result of an earlier scan.
//The result is embedded in a bit of JSON parsed by the javascript in wifi.tpl.
int ICACHE_FLASH_ATTR cgiWiFiScan(HttpdConnData *connData) {
	int pos=(int)connData->cgiData;
	int len;
	char buff[1024];

	if (!cgiWifiAps.scanInProgress && pos!=0) {
		//Fill in json code for an access point
		if (pos-1<cgiWifiAps.noAps) {
			len=os_sprintf(buff, "{\"essid\": \"%s\", \"rssi\": \"%d\", \"enc\": \"%d\"}%s\n",
					cgiWifiAps.apData[pos-1]->ssid, cgiWifiAps.apData[pos-1]->rssi,
					cgiWifiAps.apData[pos-1]->enc, (pos-1==cgiWifiAps.noAps-1)?"":",");
			httpdSend(connData, buff, len);
		}
		pos++;
		if ((pos-1)>=cgiWifiAps.noAps) {
			len=os_sprintf(buff, "]\n}\n}\n");
			httpdSend(connData, buff, len);
			//Also start a new scan.
			wifiStartScan();
			return HTTPD_CGI_DONE;
		} else {
			connData->cgiData=(void*)pos;
			return HTTPD_CGI_MORE;
		}
	}

	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Content-Type", "text/json");
	httpdEndHeaders(connData);

	if (cgiWifiAps.scanInProgress==1) {
		//We're still scanning. Tell Javascript code that.
		len=os_sprintf(buff, "{\n \"result\": { \n\"inProgress\": \"1\"\n }\n}\n");
		httpdSend(connData, buff, len);
		return HTTPD_CGI_DONE;
	} else {
		//We have a scan result. Pass it on.
		len=os_sprintf(buff, "{\n \"result\": { \n\"inProgress\": \"0\", \n\"APs\": [\n");
		httpdSend(connData, buff, len);
		if (cgiWifiAps.apData==NULL) cgiWifiAps.noAps=0;
		connData->cgiData=(void *)1;
		return HTTPD_CGI_MORE;
	}
}

// ===== timers to change state and rescue from failed associations

//#define CONNTRY_IDLE 0
//#define CONNTRY_WORKING 1
//#define CONNTRY_SUCCESS 2
//#define CONNTRY_FAIL 3
//Connection result var
//static int connTryStatus=CONNTRY_IDLE;

// reset timer changes back to STA+AP if we can't associate
#define RESET_TIMEOUT (15000) // 15 seconds
static ETSTimer resetTimer;

//This routine is ran some time after a connection attempt to an access point. If
//the connect succeeds, this gets the module in STA-only mode.
static void ICACHE_FLASH_ATTR resetTimerCb(void *arg) {
	int x = wifi_station_get_connect_status();
	int m = wifi_get_opmode();
	os_printf("Wifi check: mode=%d status=%d\n", m, x);

	if (x == STATION_GOT_IP) {
		if (m != 1) {
			// We're happily connected, go to STA mode
			os_printf("Wifi got IP. Going into STA mode..\n");
			wifi_set_opmode(1);
			os_timer_arm(&resetTimer, RESET_TIMEOUT, 0);
		}
	} else if (m != 3) {
		os_printf("Wifi connect failed. Going into STA+AP mode..\n");
		wifi_set_opmode(3);
		os_timer_arm(&resetTimer, RESET_TIMEOUT, 0);
	}
}

// Temp store for new ap info.
static struct station_config stconf;
// Reassociate timer to delay change of association so the original request can finish
static ETSTimer reassTimer;

// Callback actually doing reassociation
static void ICACHE_FLASH_ATTR reassTimerCb(void *arg) {
	os_printf("Wifi changing association\n");
	wifi_station_disconnect();
	wifi_station_set_config(&stconf);
	wifi_station_connect();
	// Schedule check
	os_timer_disarm(&resetTimer);
	os_timer_setfn(&resetTimer, resetTimerCb, NULL);
	os_timer_arm(&resetTimer, RESET_TIMEOUT, 0);
}

//This cgi uses the routines above to connect to a specific access point with the
//given ESSID using the given password.
int ICACHE_FLASH_ATTR cgiWiFiConnect(HttpdConnData *connData) {
	char essid[128];
	char passwd[128];

	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	httpdFindArg(connData->post->buff, "essid", essid, sizeof(essid));
	httpdFindArg(connData->post->buff, "passwd", passwd, sizeof(passwd));

	os_strncpy((char*)stconf.ssid, essid, 32);
	os_strncpy((char*)stconf.password, passwd, 64);
	os_printf("Wifi try to connect to AP %s pw %s\n", essid, passwd);

	//Schedule disconnect/connect
	os_timer_disarm(&reassTimer);
	os_timer_setfn(&reassTimer, reassTimerCb, NULL);
//Set to 0 if you want to disable the actual reconnecting bit
#ifdef DEMO_MODE
	httpdRedirect(connData, "/wifi");
#else
	os_timer_arm(&reassTimer, 1000, 0);
	httpdRedirect(connData, "connecting.html");
#endif
	return HTTPD_CGI_DONE;
}

//This cgi changes the operating mode: STA / AP / STA+AP
int ICACHE_FLASH_ATTR cgiWiFiSetMode(HttpdConnData *connData) {
	int len;
	char buff[1024];

	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	len=httpdFindArg(connData->getArgs, "mode", buff, sizeof(buff));
	if (len!=0) {
		int m = atoi(buff);
		os_printf("Wifi switching to mode %d\n", m);
#ifndef DEMO_MODE
		wifi_set_opmode(m);
		if (m == 1) {
			// STA-only mode, reset into STA+AP after a timeout
			os_timer_disarm(&resetTimer);
			os_timer_setfn(&resetTimer, resetTimerCb, NULL);
			os_timer_arm(&resetTimer, RESET_TIMEOUT, 0);
		}
#endif
	}
	httpdRedirect(connData, "/wifi");
	return HTTPD_CGI_DONE;
}

char *connStatuses[] = { "idle", "connecting", "wrong password", "AP not found",
                         "failed", "got IP address" };

int ICACHE_FLASH_ATTR cgiWiFiConnStatus(HttpdConnData *connData) {
	char buff[1024];
	int len;
	struct ip_info info;
	int st=wifi_station_get_connect_status();
	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Content-Type", "text/json");
	httpdEndHeaders(connData);

	len = os_sprintf(buff, "{\"status\": \"");
	if (st > 0 && st < sizeof(connStatuses)) {
		len += os_sprintf(buff+len, connStatuses[st]);
	} else {
		len += os_sprintf(buff+len, "unknown");
	}
	if (wifiReason != 0) {
		len += os_sprintf(buff+len, " -- %s", wifiGetReason());
	}
	if (st == STATION_GOT_IP) {
		wifi_get_ip_info(0, &info);
		len+=os_sprintf(buff+len, "\", \"ip\": \"%d.%d.%d.%d\" }\n",
			(info.ip.addr>>0)&0xff, (info.ip.addr>>8)&0xff,
			(info.ip.addr>>16)&0xff, (info.ip.addr>>24)&0xff);
		if (wifi_get_opmode() != 1) {
			//Reset into AP-only mode sooner.
			os_timer_disarm(&resetTimer);
			os_timer_setfn(&resetTimer, resetTimerCb, NULL);
			os_timer_arm(&resetTimer, 1000, 0);
		}
	} else {
		len+=os_sprintf(buff+len, "\" }\n");
	}

	buff[len] = 0;
	os_printf("  -> %s\n", buff);
	httpdSend(connData, buff, len);
	return HTTPD_CGI_DONE;
}

//Template code for the WLAN page.
int ICACHE_FLASH_ATTR tplWlan(HttpdConnData *connData, char *token, void **arg) {
	char buff[1024];
	int x;
	static struct station_config stconf;
	if (token==NULL) return HTTPD_CGI_DONE;
	wifi_station_get_config(&stconf);

	os_strcpy(buff, "Unknown");
	if (os_strcmp(token, "WiFiMode")==0) {
		x=wifi_get_opmode();
		if (x==1) os_strcpy(buff, "Client");
		if (x==2) os_strcpy(buff, "SoftAP");
		if (x==3) os_strcpy(buff, "STA+AP");
	} else if (os_strcmp(token, "currSsid")==0) {
		os_strcpy(buff, (char*)stconf.ssid);
	} else if (os_strcmp(token, "currStatus")==0) {
		int st=wifi_station_get_connect_status();
		if (st > 0 && st < sizeof(connStatuses))
			os_strcpy(buff, connStatuses[st]);
		else
			os_strcpy(buff, "unknown");
	} else if (os_strcmp(token, "currPhy")==0) {
		int m = wifi_get_phy_mode();
		os_sprintf(buff, "%d", m);
	} else if (os_strcmp(token, "WiFiPasswd")==0) {
		os_strcpy(buff, (char*)stconf.password);
	} else if (os_strcmp(token, "WiFiapwarn")==0) {
		x=wifi_get_opmode();
		switch (x) {
		case 1:
			os_strcpy(buff, "Click <a href=\"setmode.cgi?mode=3\">here</a> to go to STA+AP mode.");
			break;
		case 2:
			os_strcpy(buff, "<b>Can't scan in this mode.</b> Click <a href=\"setmode.cgi?mode=3\">here</a> to go to STA+AP mode.");
			break;
		case 3:
			os_strcpy(buff, "Click <a href=\"setmode.cgi?mode=1\">here</a> to go to STA mode.");
			break;
		}
	}
	httpdSend(connData, buff, -1);
	return HTTPD_CGI_DONE;
}

// Init the wireless, which consists of setting a timer if we expect to connect to an AP
// so we can revert to STA+AP mode if we can't connect.
void ICACHE_FLASH_ATTR wifiInit() {
	int x = wifi_get_opmode();
	os_printf("Wifi init, mode=%d\n", x);
	wifi_set_phy_mode(2);
	wifi_set_event_handler_cb(wifiHandleEventCb);
	if (x == 1) {
		// STA-only mode, reset into STA+AP after a timeout
		os_timer_disarm(&resetTimer);
		os_timer_setfn(&resetTimer, resetTimerCb, NULL);
		os_timer_arm(&resetTimer, RESET_TIMEOUT, 0);
	}
}

