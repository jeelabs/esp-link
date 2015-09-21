#ifndef STATUS_H
#define STATUS_H

int mqttStatusMsg(char *buf);
void statusWifiUpdate(uint8_t state);
void statusInit(void);

#endif

