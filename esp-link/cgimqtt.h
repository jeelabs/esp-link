#ifdef MQTT
#ifndef CGIMQTT_H
#define CGIMQTT_H

#include "httpd.h"
int cgiMqtt(HttpdConnData *connData);
char *mqttState(void);

#endif // CGIMQTT_H
#endif // MQTT
