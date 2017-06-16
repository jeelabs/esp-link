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
 * Heavily modified and enhanced by Thorsten von Eicken in 2015
 * ----------------------------------------------------------------------------
 */


#include <esp8266.h>
#include "cgiwifi.h"
#include "cgi.h"
#include "status.h"
#include "config.h"
#include "log.h"

#ifdef CGIWIFI_DBG
#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)
#else
#define DBG(format, ...) do { } while(0)
#endif

# define VERS_STR_STR(V) #V
# define VERS_STR(V) VERS_STR_STR(V)

bool mdns_started = false;

// ===== wifi status change callbacks
static WifiStateChangeCb wifi_state_change_cb[4];

// Temp store for new station config
struct station_config stconf;

// Temp store for new ap config
struct softap_config apconf;

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

static char *wifiMode[] = { 0, "STA", "AP", "AP+STA" };
static char *wifiPhy[]  = { 0, "11b", "11g", "11n" };

void (*wifiStatusCb)(uint8_t); // callback when wifi status changes

static char* ICACHE_FLASH_ATTR wifiGetReason(void) {
  if (wifiReason <= 24) return wifiReasons[wifiReason];
  if (wifiReason >= 200 && wifiReason <= 201) return wifiReasons[wifiReason-200+24];
  return wifiReasons[1];
}

// handler for wifi status change callback coming in from espressif library
static void ICACHE_FLASH_ATTR wifiHandleEventCb(System_Event_t *evt) {
  switch (evt->event) {
  case EVENT_STAMODE_CONNECTED:
    wifiState = wifiIsConnected;
    wifiReason = 0;
    DBG("Wifi connected to ssid %s, ch %d\n", evt->event_info.connected.ssid,
      evt->event_info.connected.channel);
    statusWifiUpdate(wifiState);
    break;
  case EVENT_STAMODE_DISCONNECTED:
    wifiState = wifiIsDisconnected;
    wifiReason = evt->event_info.disconnected.reason;
    DBG("Wifi disconnected from ssid %s, reason %s (%d)\n",
      evt->event_info.disconnected.ssid, wifiGetReason(), evt->event_info.disconnected.reason);
    statusWifiUpdate(wifiState);
    break;
  case EVENT_STAMODE_AUTHMODE_CHANGE:
    DBG("Wifi auth mode: %d -> %d\n",
      evt->event_info.auth_change.old_mode, evt->event_info.auth_change.new_mode);
    break;
  case EVENT_STAMODE_GOT_IP:
    wifiState = wifiGotIP;
    wifiReason = 0;
    DBG("Wifi got ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR "\n",
      IP2STR(&evt->event_info.got_ip.ip), IP2STR(&evt->event_info.got_ip.mask),
      IP2STR(&evt->event_info.got_ip.gw));
    statusWifiUpdate(wifiState);
    if (!mdns_started)
      wifiStartMDNS(evt->event_info.got_ip.ip);
    break;
  case EVENT_SOFTAPMODE_STACONNECTED:
    DBG("Wifi AP: station " MACSTR " joined, AID = %d\n",
        MAC2STR(evt->event_info.sta_connected.mac), evt->event_info.sta_connected.aid);
    break;
  case EVENT_SOFTAPMODE_STADISCONNECTED:
    DBG("Wifi AP: station " MACSTR " left, AID = %d\n",
        MAC2STR(evt->event_info.sta_disconnected.mac), evt->event_info.sta_disconnected.aid);
    break;
  default:
    break;
  }

  for (int i = 0; i < 4; i++) {
    if (wifi_state_change_cb[i] != NULL) (wifi_state_change_cb[i])(wifiState);
  }
}

void ICACHE_FLASH_ATTR wifiAddStateChangeCb(WifiStateChangeCb cb) {
  for (int i = 0; i < 4; i++) {
    if (wifi_state_change_cb[i] == cb) return;
    if (wifi_state_change_cb[i] == NULL) {
      wifi_state_change_cb[i] = cb;
      return;
    }
  }
  DBG("WIFI: max state change cb count exceeded\n");
}

static struct mdns_info *mdns_info;
// See https://github.com/arduino/Arduino/blob/master/arduino-core/src/cc/arduino/packages/discoverers/NetworkDiscovery.java#L155-L168
static char* mdns_txt = "ssh_upload=no";

void ICACHE_FLASH_ATTR wifiStartMDNS(struct ip_addr ip) {
  if (flashConfig.mdns_enable) {
    if (mdns_info == NULL)
      mdns_info = (struct mdns_info *)os_zalloc(sizeof(struct mdns_info));

    mdns_info->host_name = flashConfig.hostname;
    mdns_info->server_name = flashConfig.mdns_servername;
    mdns_info->server_port = 80;
    mdns_info->ipAddr = ip.addr;
    mdns_info->txt_data[0] = mdns_txt;
    espconn_mdns_init(mdns_info);
  }
  else {
    espconn_mdns_server_unregister();
    espconn_mdns_close();
    if (mdns_info != NULL) {
      os_free(mdns_info);
      mdns_info = NULL;
    }
  }
  mdns_started = true;
}

// ===== wifi scanning

//WiFi access point data
typedef struct {
  char ssid[32];
  sint8 rssi;
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
    DBG("wifiScanDoneCb status=%d\n", status);
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
  DBG("Scan done: found %d APs\n", n);

  //Copy access point data to the static struct
  n=0;
  bss_link = (struct bss_info *)arg;
  while (bss_link != NULL) {
    if (n>=cgiWifiAps.noAps) {
      //This means the bss_link changed under our nose. Shouldn't happen!
      //Break because otherwise we will write in unallocated memory.
      DBG("Huh? I have more than the allocated %d aps!\n", cgiWifiAps.noAps);
      break;
    }
    //Save the ap data.
    cgiWifiAps.apData[n]=(ApData *)os_malloc(sizeof(ApData));
    cgiWifiAps.apData[n]->rssi=bss_link->rssi;
    cgiWifiAps.apData[n]->enc=bss_link->authmode;
    strncpy(cgiWifiAps.apData[n]->ssid, (char*)bss_link->ssid, 32);
    DBG("bss%d: %s (%d)\n", n+1, (char*)bss_link->ssid, bss_link->rssi);

    bss_link = bss_link->next.stqe_next;
    n++;
  }
  //We're done.
  cgiWifiAps.scanInProgress=0;
}

static ETSTimer scanTimer;
static void ICACHE_FLASH_ATTR scanStartCb(void *arg) {
  DBG("Starting a scan\n");
  wifi_station_scan(NULL, wifiScanDoneCb);
}

static int ICACHE_FLASH_ATTR cgiWiFiGetScan(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  char buff[1460];
  const int chunk = 1460/64; // ssid is up to 32 chars
  int len = 0;

  DBG("GET scan: cgiData=%d noAps=%d\n", (int)connData->cgiData, cgiWifiAps.noAps);

  // handle continuation call, connData->cgiData-1 is the position in the scan results where we
  // we need to continue sending from (using -1 'cause 0 means it's the first call)
  if (connData->cgiData) {
    int next = (int)connData->cgiData-1;
    int pos = next;
    while (pos < cgiWifiAps.noAps && pos < next+chunk) {
      len += os_sprintf(buff+len, "{\"essid\": \"%s\", \"rssi\": %d, \"enc\": \"%d\"}%c\n",
        cgiWifiAps.apData[pos]->ssid, cgiWifiAps.apData[pos]->rssi, cgiWifiAps.apData[pos]->enc,
        (pos+1 == cgiWifiAps.noAps) ? ' ' : ',');
      pos++;
    }
    // done or more?
    if (pos == cgiWifiAps.noAps) {
      len += os_sprintf(buff+len, "]}}\n");
      httpdSend(connData, buff, len);
      return HTTPD_CGI_DONE;
    } else {
      connData->cgiData = (void*)(pos+1);
      httpdSend(connData, buff, len);
      return HTTPD_CGI_MORE;
    }
  }

  jsonHeader(connData, 200);

  if (cgiWifiAps.scanInProgress==1) {
    //We're still scanning. Tell Javascript code that.
    len = os_sprintf(buff, "{\n \"result\": { \n\"inProgress\": \"1\"\n }\n}\n");
    httpdSend(connData, buff, len);
    return HTTPD_CGI_DONE;
  }

  len = os_sprintf(buff, "{\"result\": {\"inProgress\": \"0\", \"APs\": [\n");
  connData->cgiData = (void *)1; // start with first result next time we're called
  httpdSend(connData, buff, len);
  return HTTPD_CGI_MORE;
}

// Start scanning, without parameters
void ICACHE_FLASH_ATTR wifiStartScan() {
  if (!cgiWifiAps.scanInProgress) {
    cgiWifiAps.scanInProgress = 1;
    os_timer_disarm(&scanTimer);
    os_timer_setfn(&scanTimer, scanStartCb, NULL);
    os_timer_arm_us(&scanTimer, 200000, 0);
  }
}

// Start scanning, web interface
int ICACHE_FLASH_ATTR cgiWiFiStartScan(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  jsonHeader(connData, 200);

  // Don't duplicate code, reuse the function above.
  wifiStartScan();

  return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiWiFiScan(HttpdConnData *connData) {
    if (connData->requestType == HTTPD_METHOD_GET) {
        return cgiWiFiGetScan(connData);
    }else if(connData->requestType == HTTPD_METHOD_POST) {
        // DO NOT start APs scan in AP mode
        int mode = wifi_get_opmode();
        if(mode==2){
            jsonHeader(connData, 400);
            return HTTPD_CGI_DONE;
        }else{
            return cgiWiFiStartScan(connData);
        }
    }else{
        jsonHeader(connData, 404);
        return HTTPD_CGI_DONE;
    }
}

// ===== timers to change state and rescue from failed associations

// reset timer changes back to STA+AP if we can't associate
#define RESET_TIMEOUT (15000) // 15 seconds
static ETSTimer resetTimer;

// This routine is ran some time after a connection attempt to an access point. If
// the connect succeeds, this gets the module in STA-only mode. If it fails, it ensures
// that the module is in STA+AP mode so the user has a chance to recover.
static void ICACHE_FLASH_ATTR resetTimerCb(void *arg) {
  int x = wifi_station_get_connect_status();
  int m = wifi_get_opmode() & 0x3;
  DBG("Wifi check: mode=%s status=%d\n", wifiMode[m], x);

  if (m == 2) return; // 2=AP, in AP-only mode we don't do any auto-switching

  if ( x == STATION_GOT_IP ) {
    // if we got an IP we could switch to STA-only...
    if (m != 1) { // 1=STA
#ifdef CHANGE_TO_STA
      // We're happily connected, go to STA mode
      DBG("Wifi got IP. Going into STA mode..\n");
      wifi_set_opmode(1);
      os_timer_arm_us(&resetTimer, RESET_TIMEOUT * 1000, 0); // check one more time after switching to STA-only
#endif
    }
    log_uart(false);
    // no more resetTimer at this point, gotta use physical reset to recover if in trouble
  } else {
    // we don't have an IP address
    if (m != 3) {
      DBG("Wifi connect failed. Going into STA+AP mode..\n");
      wifi_set_opmode(3);
      wifi_softap_set_config(&apconf);
    }
    log_uart(true);
    DBG("Enabling/continuing uart log\n");
    os_timer_arm_us(&resetTimer, RESET_TIMEOUT * 1000, 0);
  }
}

// Reassociate timer to delay change of association so the original request can finish
static ETSTimer reassTimer;

// Callback actually doing reassociation
static void ICACHE_FLASH_ATTR reassTimerCb(void *arg) {
  DBG("Wifi changing association\n");
  wifi_station_disconnect();
  stconf.bssid_set = 0;
  wifi_station_set_config(&stconf);
  wifi_station_connect();
  // Schedule check, we give some extra time (4x) 'cause the reassociation can cause the AP
  // to have to change channel, and then the client needs to follow before it can see the
  // IP address
  os_timer_disarm(&resetTimer);
  os_timer_setfn(&resetTimer, resetTimerCb, NULL);
  os_timer_arm_us(&resetTimer, 4*RESET_TIMEOUT * 1000, 0);
}

// Kick off connection to some network
void ICACHE_FLASH_ATTR connectToNetwork(char *ssid, char *pass) {
  os_strncpy((char*)stconf.ssid, ssid, 32);
  os_strncpy((char*)stconf.password, pass, 64);
  DBG("Wifi try to connect to AP %s pw %s\n", ssid, pass);

  // Schedule disconnect/connect
  os_timer_disarm(&reassTimer);
  os_timer_setfn(&reassTimer, reassTimerCb, NULL);
  os_timer_arm_us(&reassTimer, 1000000, 0); // 1 second for the response of this request to make it
}

// This cgi uses the routines above to connect to a specific access point with the
// given ESSID using the given password.
int ICACHE_FLASH_ATTR cgiWiFiConnect(HttpdConnData *connData) {
    int mode = wifi_get_opmode();
    if(mode == 2){
        jsonHeader(connData, 400);
        httpdSend(connData, "Can't associate to an AP en SoftAP mode", -1);
        return HTTPD_CGI_DONE;
    }
  char essid[128];
  char passwd[128];

  if (connData->conn==NULL) return HTTPD_CGI_DONE;

  int el = httpdFindArg(connData->getArgs, "essid", essid, sizeof(essid));
  int pl = httpdFindArg(connData->getArgs, "passwd", passwd, sizeof(passwd));

  if (el > 0 && pl >= 0) {
    //Set to 0 if you want to disable the actual reconnecting bit

    connectToNetwork(essid, passwd);
    jsonHeader(connData, 200);
  } else {
    jsonHeader(connData, 400);
    httpdSend(connData, "Cannot parse ssid or password", -1);
  }
  return HTTPD_CGI_DONE;
}

static bool ICACHE_FLASH_ATTR parse_ip(char *buff, ip_addr_t *ip_ptr) {
  char *next = buff; // where to start parsing next integer
  int found = 0;     // number of integers parsed
  uint32_t ip = 0;   // the ip addres parsed
  for (int i=0; i<32; i++) { // 32 is just a safety limit
    char c = buff[i];
    if (c == '.' || c == 0) {
      // parse the preceding integer and accumulate into IP address
      bool last = c == 0;
      buff[i] = 0;
      uint32_t v = atoi(next);
      ip = ip | ((v&0xff)<<(found*8));
      next = buff+i+1; // next integer starts after the '.'
      found++;
      if (last) { // if at end of string we better got 4 integers
        ip_ptr->addr = ip;
        return found == 4;
      }
      continue;
    }
    if (c < '0' || c > '9') return false;
  }
  return false;
}

#ifdef DEBUGIP
static void ICACHE_FLASH_ATTR debugIP() {
  struct ip_info info;
  if (wifi_get_ip_info(0, &info)) {
    DBG("\"ip\": \"%d.%d.%d.%d\"\n", IP2STR(&info.ip.addr));
    DBG("\"netmask\": \"%d.%d.%d.%d\"\n", IP2STR(&info.netmask.addr));
    DBG("\"gateway\": \"%d.%d.%d.%d\"\n", IP2STR(&info.gw.addr));
    DBG("\"hostname\": \"%s\"\n", wifi_station_get_hostname());
  } else {
    DBG("\"ip\": \"-none-\"\n");
  }
}
#endif

// configure Wifi, specifically DHCP vs static IP address based on flash config
void ICACHE_FLASH_ATTR configWifiIP() {
  if (flashConfig.staticip == 0) {
    // let's DHCP!
    wifi_station_set_hostname(flashConfig.hostname);
    if (wifi_station_dhcpc_status() == DHCP_STARTED)
      wifi_station_dhcpc_stop();
    wifi_station_dhcpc_start();
    DBG("Wifi uses DHCP, hostname=%s\n", flashConfig.hostname);
  } else {
    // no DHCP, we got static network config!
    wifi_station_dhcpc_stop();
    struct ip_info ipi;
    ipi.ip.addr = flashConfig.staticip;
    ipi.netmask.addr = flashConfig.netmask;
    ipi.gw.addr = flashConfig.gateway;
    wifi_set_ip_info(0, &ipi);
    DBG("Wifi uses static IP %d.%d.%d.%d\n", IP2STR(&ipi.ip.addr));
  }
#ifdef DEBUGIP
  debugIP();
#endif
}

// Change special settings
int ICACHE_FLASH_ATTR cgiWiFiSpecial(HttpdConnData *connData) {
  char dhcp[8];
  char staticip[20];
  char netmask[20];
  char gateway[20];

  if (connData->conn==NULL) return HTTPD_CGI_DONE;

  // get args and their string lengths
  int dl = httpdFindArg(connData->getArgs, "dhcp", dhcp, sizeof(dhcp));
  int sl = httpdFindArg(connData->getArgs, "staticip", staticip, sizeof(staticip));
  int nl = httpdFindArg(connData->getArgs, "netmask", netmask, sizeof(netmask));
  int gl = httpdFindArg(connData->getArgs, "gateway", gateway, sizeof(gateway));

  if (!(dl > 0 && sl >= 0 && nl >= 0 && gl >= 0)) {
    jsonHeader(connData, 400);
    httpdSend(connData, "Request is missing fields", -1);
    return HTTPD_CGI_DONE;
  }

  char url[64]; // redirect URL
  if (os_strcmp(dhcp, "off") == 0) {
    // parse static IP params
    struct ip_info ipi;
    bool ok = parse_ip(staticip, &ipi.ip);
    if (nl > 0) ok = ok && parse_ip(netmask, &ipi.netmask);
    else IP4_ADDR(&ipi.netmask, 255, 255, 255, 0);
    if (gl > 0) ok = ok && parse_ip(gateway, &ipi.gw);
    else ipi.gw.addr = 0;
    if (!ok) {
      jsonHeader(connData, 400);
      httpdSend(connData, "Cannot parse static IP config", -1);
      return HTTPD_CGI_DONE;
    }
    // save the params in flash
    flashConfig.staticip = ipi.ip.addr;
    flashConfig.netmask = ipi.netmask.addr;
    flashConfig.gateway = ipi.gw.addr;
    // construct redirect URL
    os_sprintf(url, "{\"url\": \"http://%d.%d.%d.%d\"}", IP2STR(&ipi.ip));

  } else {
    // dynamic IP
    flashConfig.staticip = 0;
    os_sprintf(url, "{\"url\": \"http://%s\"}", flashConfig.hostname);
  }

  configSave(); // ignore error...
  // schedule change-over
  os_timer_disarm(&reassTimer);
  os_timer_setfn(&reassTimer, configWifiIP, NULL);
  os_timer_arm_us(&reassTimer, 1 * 1000000, 0); // 1 second for the response of this request to make it
  // return redirect info
  jsonHeader(connData, 200);
  httpdSend(connData, url, -1);
  return HTTPD_CGI_DONE;
}

// ==== Soft-AP related functions

// Change Soft-AP main settings
int ICACHE_FLASH_ATTR cgiApSettingsChange(HttpdConnData *connData) {

    if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

    // No changes for Soft-AP in STA mode
    int mode = wifi_get_opmode();
    if ( mode == 1 ){
        jsonHeader(connData, 400);
        httpdSend(connData, "No changes allowed in STA mode", -1);
        return HTTPD_CGI_DONE;
    }

    char buff[96];
    int len;

    // Check extra security measure, this must be 1
    len=httpdFindArg(connData->getArgs, "100", buff, sizeof(buff));
    if(len>0){
        if(atoi(buff)!=1){
            jsonHeader(connData, 400);
            return HTTPD_CGI_DONE;
        }
    }
    // Set new SSID
    len=httpdFindArg(connData->getArgs, "ap_ssid", buff, sizeof(buff));
    if(checkString(buff) && len>0 && len<=32){
        // STRING PREPROCESSING DONE IN CLIENT SIDE
        os_memset(apconf.ssid, 0, 32);
        os_memcpy(apconf.ssid, buff, len);
        apconf.ssid_len = len;
    }else{
        jsonHeader(connData, 400);
        httpdSend(connData, "SSID not valid or out of range", -1);
        return HTTPD_CGI_DONE;
    }
    // Set new PASSWORD
    len=httpdFindArg(connData->getArgs, "ap_password", buff, sizeof(buff));
    os_memset(apconf.password, 0, 64);
    if (checkString(buff) && len>7 && len<=64) {
        // String preprocessing done in client side, wifiap.js line 31
        os_memcpy(apconf.password, buff, len);
        DBG("Setting AP password len=%d\n", len);
    } else if (len != 0) {
        jsonHeader(connData, 400);
        httpdSend(connData, "PASSWORD not valid or out of range", -1);
        return HTTPD_CGI_DONE;
    }
    // Set auth mode
    if (len != 0) {
        // Set authentication mode, before password to check open settings
        len=httpdFindArg(connData->getArgs, "ap_authmode", buff, sizeof(buff));
        if (len > 0) {
            int value = atoi(buff);
            if (value > 0  && value <= 4) {
                apconf.authmode = value;
            } else {
                // If out of range set by default
                DBG("Forcing AP authmode to WPA_WPA2_PSK\n");
                apconf.authmode = 4;
            }
        } else {
            // Valid password but wrong auth mode, default 4
            DBG("Forcing AP authmode to WPA_WPA2_PSK\n");
            apconf.authmode = 4;
        }
    } else {
        apconf.authmode = 0;
    }
    DBG("Setting AP authmode=%d\n", apconf.authmode);
    // Set max connection number
    len=httpdFindArg(connData->getArgs, "ap_maxconn", buff, sizeof(buff));
    if(len>0){

        int value = atoi(buff);
        if(value > 0 && value <= 4){
            apconf.max_connection = value;
        }else{
            // If out of range, set by default
            apconf.max_connection = 4;
        }
    }
    // Set beacon interval value
    len=httpdFindArg(connData->getArgs, "ap_beacon", buff, sizeof(buff));
    if(len>0){
        int value = atoi(buff);
        if(value >= 100 && value <= 60000){
            apconf.beacon_interval = value;
        }else{
            // If out of range, set by default
            apconf.beacon_interval = 100;
        }
    }
    // Set ssid to be hidden or not
    len=httpdFindArg(connData->getArgs, "ap_hidden", buff, sizeof(buff));
    if(len>0){
        int value = atoi(buff);
        if(value == 0  || value == 1){
            apconf.ssid_hidden = value;
        }else{
            // If out of range, set by default
            apconf.ssid_hidden = 0;
        }
    }
    // Store new configuration
    wifi_softap_set_config(&apconf);

    jsonHeader(connData, 200);
    return HTTPD_CGI_DONE;
}

// Get current Soft-AP settings
int ICACHE_FLASH_ATTR cgiApSettingsInfo(HttpdConnData *connData) {

    char buff[1024];
    if (connData->conn == NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
    os_sprintf(buff,
               "{ "
               "\"ap_ssid\": \"%s\", "
               "\"ap_password\": \"%s\", "
               "\"ap_authmode\": %d, "
               "\"ap_maxconn\": %d, "
               "\"ap_beacon\": %d, "
               "\"ap_hidden\": \"%s\" "
               " }",
               apconf.ssid,
               apconf.password,
               apconf.authmode,
               apconf.max_connection,
               apconf.beacon_interval,
               apconf.ssid_hidden ? "enabled" : "disabled"
               );

    jsonHeader(connData, 200);
    httpdSend(connData, buff, -1);
    return HTTPD_CGI_DONE;
}

//This cgi changes the operating mode: STA / AP / STA+AP
int ICACHE_FLASH_ATTR cgiWiFiSetMode(HttpdConnData *connData) {
  int len;
  char buff[1024];
  int previous_mode = wifi_get_opmode();
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

  len=httpdFindArg(connData->getArgs, "mode", buff, sizeof(buff));
    int next_mode = atoi(buff);

    if (len!=0) {
        if (next_mode == 2){
            // moving to AP mode, so disconnect before leave STA mode
            wifi_station_disconnect();
        }

        DBG("Wifi switching to mode %d\n", next_mode);
        wifi_set_opmode(next_mode&3);

        if (previous_mode == 2) {
            // moving to STA or STA+AP mode from AP, try to connect and set timer
            stconf.bssid_set = 0;
            wifi_station_set_config(&stconf);
            wifi_station_connect();
            os_timer_disarm(&resetTimer);
            os_timer_setfn(&resetTimer, resetTimerCb, NULL);
            os_timer_arm_us(&resetTimer, RESET_TIMEOUT * 1000, 0);
        }
        if(previous_mode == 1){
            // moving to AP or STA+AP from STA, so softap config call needed
            wifi_softap_set_config(&apconf);
        }
        jsonHeader(connData, 200);
    } else {
        jsonHeader(connData, 400);
    }
    return HTTPD_CGI_DONE;
}

static char *connStatuses[] = { "idle", "connecting", "wrong password", "AP not found",
                         "failed", "got IP address" };

static char *wifiWarn[] = { 0,
    "Switch to <a href=\\\"#\\\" onclick=\\\"changeWifiMode(3)\\\">STA+AP mode</a>",
    "Switch to <a href=\\\"#\\\" onclick=\\\"changeWifiMode(3)\\\">STA+AP mode</a>",
    "Switch to <a href=\\\"#\\\" onclick=\\\"changeWifiMode(1)\\\">STA mode</a>",
    "Switch to <a href=\\\"#\\\" onclick=\\\"changeWifiMode(2)\\\">AP mode</a>",
};

static char *apAuthMode[] = { "OPEN",
    "WEP",
    "WPA_PSK",
    "WPA2_PSK",
    "WPA_WPA2_PSK",
};

#ifdef CHANGE_TO_STA
#define MODECHANGE "yes"
#else
#define MODECHANGE "no"
#endif

// print various Wifi information into json buffer
int ICACHE_FLASH_ATTR printWifiInfo(char *buff) {
  int len;
    //struct station_config stconf;
    wifi_station_get_config(&stconf);
    //struct softap_config apconf;
    wifi_softap_get_config(&apconf);

    uint8_t op = wifi_get_opmode() & 0x3;
    char *mode = wifiMode[op];
    char *status = "unknown";
    int st = wifi_station_get_connect_status();
    if (st >= 0 && st < sizeof(connStatuses)) status = connStatuses[st];
    int p = wifi_get_phy_mode();
    char *phy = wifiPhy[p&3];
    char *warn = wifiWarn[op];
    if (op == 3) op = 4; // Done to let user switch to AP only mode from Soft-AP settings page, using only one set of warnings
    char *apwarn = wifiWarn[op];
    char *apauth = apAuthMode[apconf.authmode];
    sint8 rssi = wifi_station_get_rssi();
    if (rssi > 0) rssi = 0;
    uint8 mac_addr[6];
    uint8 apmac_addr[6];
    wifi_get_macaddr(0, mac_addr);
    wifi_get_macaddr(1, apmac_addr);
    uint8_t chan = wifi_get_channel();

    len = os_sprintf(buff,
        "\"mode\": \"%s\", \"modechange\": \"%s\", \"ssid\": \"%s\", \"status\": \"%s\", "
        "\"phy\": \"%s\", \"rssi\": \"%ddB\", \"warn\": \"%s\",  \"apwarn\": \"%s\", "
        "\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\", \"chan\":\"%d\", \"apssid\": \"%s\", "
        "\"appass\": \"%s\", \"apchan\": \"%d\", \"apmaxc\": \"%d\", \"aphidd\": \"%s\", "
        "\"apbeac\": \"%d\", \"apauth\": \"%s\",\"apmac\":\"%02x:%02x:%02x:%02x:%02x:%02x\"",
        mode, MODECHANGE, (char*)stconf.ssid, status, phy, rssi, warn, apwarn,
        mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5],
        chan, (char*)apconf.ssid, (char*)apconf.password, apconf.channel, apconf.max_connection,
        apconf.ssid_hidden?"enabled":"disabled", apconf.beacon_interval,
        apauth,apmac_addr[0], apmac_addr[1], apmac_addr[2], apmac_addr[3], apmac_addr[4],
        apmac_addr[5]);

    struct ip_info info;
    if (wifi_get_ip_info(0, &info)) {
        len += os_sprintf(buff+len, ", \"ip\": \"%d.%d.%d.%d\"", IP2STR(&info.ip.addr));
        len += os_sprintf(buff+len, ", \"netmask\": \"%d.%d.%d.%d\"", IP2STR(&info.netmask.addr));
        len += os_sprintf(buff+len, ", \"gateway\": \"%d.%d.%d.%d\"", IP2STR(&info.gw.addr));
        len += os_sprintf(buff+len, ", \"hostname\": \"%s\"", flashConfig.hostname);
    } else {
        len += os_sprintf(buff+len, ", \"ip\": \"-none-\"");
    }
    len += os_sprintf(buff+len, ", \"staticip\": \"%d.%d.%d.%d\"", IP2STR(&flashConfig.staticip));
    len += os_sprintf(buff+len, ", \"dhcp\": \"%s\"", flashConfig.staticip > 0 ? "off" : "on");

    return len;
}

int ICACHE_FLASH_ATTR cgiWiFiConnStatus(HttpdConnData *connData) {
  char buff[1024];
  int len;

  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  jsonHeader(connData, 200);

  len = os_sprintf(buff, "{");
  len += printWifiInfo(buff+len);
  len += os_sprintf(buff+len, ", ");

  if (wifiReason != 0) {
    len += os_sprintf(buff+len, "\"reason\": \"%s\", ", wifiGetReason());
  }

#if 0
  // commented out 'cause often the client that requested the change can't get a request in to
  // find out that it succeeded. Better to just wait the std 15 seconds...
  int st=wifi_station_get_connect_status();
  if (st == STATION_GOT_IP) {
    if (wifi_get_opmode() != 1) {
      // Reset into AP-only mode sooner.
      os_timer_disarm(&resetTimer);
      os_timer_setfn(&resetTimer, resetTimerCb, NULL);
      os_timer_arm_us(&resetTimer, 1 * 1000000, 0);
    }
  }
#endif

  len += os_sprintf(buff+len, "\"x\":0}\n");
  //DBG("  -> %s\n", buff);
  httpdSend(connData, buff, len);
  return HTTPD_CGI_DONE;
}

// Cgi to return various Wifi information
int ICACHE_FLASH_ATTR cgiWifiInfo(HttpdConnData *connData) {
  char buff[1024];

  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

  os_strcpy(buff, "{");
  printWifiInfo(buff+1);
  os_strcat(buff, "}");

  jsonHeader(connData, 200);
  httpdSend(connData, buff, -1);
  return HTTPD_CGI_DONE;
}

// Check string againt invalid characters
int ICACHE_FLASH_ATTR checkString(char *str){
    for(int i=0; i<os_strlen(str); i++) {
        // We allow any printable character
        if (str[i] < '!' || str[i] > '~') {
            DBG("Error: String has non alphanumeric chars\n");
            return 0;
        }
    }
    return 1;
}

/*  Init the wireless
 *
 *  Call both Soft-AP and Station default config
 *  Change values according to Makefile hard-coded variables
 *  Anyway set wifi opmode to STA+AP, it will change to STA if CHANGE_TO_STA is set to yes in Makefile
 *  Call a timer to check the STA connection
 */
void ICACHE_FLASH_ATTR wifiInit() {

    // Check the wifi opmode
    int x = wifi_get_opmode() & 0x3;

    // If STA is enabled switch to STA+AP to allow for recovery, it will then switch to STA-only
    // once it gets an IP address
    if (x == 1) wifi_set_opmode(3);

    // Call both STATION and SOFTAP default config
    wifi_station_get_config_default(&stconf);
    wifi_softap_get_config_default(&apconf);

    DBG("Wifi init, mode=%s\n",wifiMode[x]);

    // Change STATION parameters, if defined in the Makefile
#if defined(STA_SSID) && defined(STA_PASS)
    // Set parameters
    if (os_strlen((char*)stconf.ssid) == 0 && os_strlen((char*)stconf.password) == 0) {
        os_strncpy((char*)stconf.ssid, VERS_STR(STA_SSID), 32);
        os_strncpy((char*)stconf.password, VERS_STR(STA_PASS), 64);

        wifi_set_opmode(3);

        DBG("Wifi pre-config trying to connect to AP %s pw %s\n",(char*)stconf.ssid, (char*)stconf.password);

        // wifi_set_phy_mode(2); // limit to 802.11b/g 'cause n is flaky
        stconf.bssid_set = 0;
        wifi_station_set_config(&stconf);
    }
#endif

    // Change SOFT_AP settings, if defined in Makefile
#if defined(AP_SSID)
    // Check if ssid and pass are alphanumeric values
    int ssidlen = os_strlen(VERS_STR(AP_SSID));
    if(checkString(VERS_STR(AP_SSID)) && ssidlen > 7 && ssidlen < 32){
        // Clean memory and set the value of SSID
        os_memset(apconf.ssid, 0, 32);
        os_memcpy(apconf.ssid, VERS_STR(AP_SSID), os_strlen(VERS_STR(AP_SSID)));
        // Specify the length of ssid
        apconf.ssid_len= ssidlen;
#if defined(AP_PASS)
        // If pass is at least 8 and less than 64
        int passlen = os_strlen(VERS_STR(AP_PASS));
        if( checkString(VERS_STR(AP_PASS)) && passlen > 7 && passlen < 64 ){
            // Clean memory and set the value of PASS
            os_memset(apconf.password, 0, 64);
            os_memcpy(apconf.password, VERS_STR(AP_PASS), passlen);
            // Can't choose auth mode without a valid ssid and password
#ifdef AP_AUTH_MODE
            // If set, use specified auth mode
            if(AP_AUTH_MODE >= 0 && AP_AUTH_MODE <=4)
                apconf.authmode = AP_AUTH_MODE;
#else
            // If not, use WPA2
            apconf.authmode = AUTH_WPA_WPA2_PSK;
#endif
        }else if ( passlen == 0){
            // If ssid is ok and no pass, set auth open
            apconf.authmode = AUTH_OPEN;
            // Remove stored password
            os_memset(apconf.password, 0, 64);
        }
#endif
    }// end of ssid and pass check
#ifdef AP_SSID_HIDDEN
    // If set, use specified ssid hidden parameter
    if(AP_SSID_HIDDEN == 0 || AP_SSID_HIDDEN ==1)
        apconf.ssid_hidden = AP_SSID_HIDDEN;
#endif
#ifdef AP_MAX_CONN
    // If set, use specified max conn number
    if(AP_MAX_CONN > 0 && AP_MAX_CONN <5)
        apconf.max_connection = AP_MAX_CONN;
#endif
#ifdef AP_BEACON_INTERVAL
    // If set use specified beacon interval
    if(AP_BEACON_INTERVAL >= 100 && AP_BEACON_INTERVAL <= 60000)
        apconf.beacon_interval = AP_BEACON_INTERVAL;
#endif
    // Check softap config
    bool softap_set_conf = wifi_softap_set_config(&apconf);
    // Debug info

    DBG("Wifi Soft-AP parameters change: %s\n",softap_set_conf? "success":"fail");
#endif // if defined(AP_SSID)

    configWifiIP();

    // The default sleep mode should be modem_sleep, but we set it here explicitly for good
    // measure. We can't use light_sleep because that powers off everthing and we would loose
    // all connections.
    wifi_set_sleep_type(MODEM_SLEEP_T);

    wifi_set_event_handler_cb(wifiHandleEventCb);
    // check on the wifi in a few seconds to see whether we need to switch mode
    os_timer_disarm(&resetTimer);
    os_timer_setfn(&resetTimer, resetTimerCb, NULL);
    os_timer_arm_us(&resetTimer, RESET_TIMEOUT * 1000, 0);
}

// Access functions for cgiWifiAps : query the number of entries in the table
int ICACHE_FLASH_ATTR wifiGetApCount() {
  if (cgiWifiAps.scanInProgress)
    return 0;
  return cgiWifiAps.noAps;
}

// Access functions for cgiWifiAps : returns the name of a network, i is the index into the array, return stored in memory pointed to by ptr.
ICACHE_FLASH_ATTR void wifiGetApName(int i, char *ptr) {
  if (i < 0)
    return;
  if (i >= cgiWifiAps.noAps)
    return;

  if (ptr != 0)
    strncpy(ptr, cgiWifiAps.apData[i]->ssid, 32);

  DBG("AP %s\n", cgiWifiAps.apData[i]->ssid);
}

// Access functions for cgiWifiAps : returns the signal strength of network (i is index into array). Return current network strength for negative i.
ICACHE_FLASH_ATTR int wifiSignalStrength(int i) {
  sint8 rssi;

  if (i < 0 || i == 255)
    rssi = wifi_station_get_rssi();	// Current network's signal strength
  else if (i >= cgiWifiAps.noAps)
    rssi = 0;				// FIX ME
  else
    rssi = cgiWifiAps.apData[i]->rssi;	// Signal strength of any known network

  return rssi;
}
