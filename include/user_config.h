#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_

/*DEFAULT CONFIGURATIONS*/

#define MQTT_HOST				"mqtt.yourdomain.com" //or "mqtt.yourdomain.com"
#define MQTT_PORT				1883
#define MQTT_BUF_SIZE		1024
#define MQTT_KEEPALIVE	120	 /*second*/

#define MQTT_CLIENT_ID	"H_%08X" //Cuidar para não colocar valores execendentes da ESTRUTURA SYSCFG
#define MQTT_USER				"DVES_USER"
#define MQTT_PASS				"DVES_PASS"

#define STA_SSID 				"TESTE"
#define STA_PASS 				"54545"
#define STA_TYPE 				AUTH_WPA2_PSK

#define MQTT_RECONNECT_TIMEOUT 	5	/*second*/

#define DEFAULT_SECURITY		0
#define QUEUE_BUFFER_SIZE		2048

//#undef MCU_RESET_PIN
//#undef MCU_ISP_PIN
//#undef LED_CONN_PIN
//#undef LED_SERIAL_PIN
//
//#define MCU_RESET_PIN       2
//#define MCU_ISP_PIN         -1
//#define LED_CONN_PIN        -1
//#define LED_SERIAL_PIN      -1

//#define BAUD_RATE           9600
//#define HOSTNAME            "nodemcu\0                        "

#define PROTOCOL_NAMEv31	/*MQTT version 3.1 compatible with Mosquitto v0.15*/
//PROTOCOL_NAMEv311			/*MQTT version 3.11 compatible with https://eclipse.org/paho/clients/testing/*/
#endif