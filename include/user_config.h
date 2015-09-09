#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_
#include <c_types.h>
#ifdef __WIN32__
#include <_mingw.h>
#endif
//#define CMD_DBG
#define MQTT_RECONNECT_TIMEOUT 	5	// seconds
#define MQTT_BUF_SIZE		  512
#define QUEUE_BUFFER_SIZE	512

#define MQTT_HOST				"10.0.0.220" // "mqtt.yourdomain.com" or ip "10.0.0.1"
#define MQTT_PORT				1883
#define MQTT_SECURITY   0

#define MQTT_CLIENT_ID	system_get_chip_id_str() // "esp-link"
#define MQTT_USER				""
#define MQTT_PASS				""
#define MQTT_KEEPALIVE	120	 // seconds
#define MQTT_CLSESSION	true

#define PROTOCOL_NAMEv31	// MQTT version 3.1 compatible with Mosquitto v0.15/
//PROTOCOL_NAMEv311			// MQTT version 3.11 compatible with https://eclipse.org/paho/clients/testing/

extern char* esp_link_version;

extern uint8_t UTILS_StrToIP(const char* str, void *ip);

extern void ICACHE_FLASH_ATTR init(void);

extern char* ICACHE_FLASH_ATTR system_get_chip_id_str();

#endif