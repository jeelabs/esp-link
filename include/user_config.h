#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_

#define MQTT_RECONNECT_TIMEOUT 	5	// seconds
#define MQTT_BUF_SIZE		1024

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

#endif