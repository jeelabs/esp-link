char *mqttState(void);
#ifdef MQTT
#ifndef CGIMQTT_H
#define CGIMQTT_H

#include "httpd.h"
int cgiMqtt(HttpdConnData *connData);

#endif // CGIMQTT_H
#endif // MQTT
