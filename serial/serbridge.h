#ifndef __SER_BRIDGE_H__
#define __SER_BRIDGE_H__

#include <ip_addr.h>
#include <c_types.h>
#include <espconn.h>

#define MAX_CONN 4
#define SER_BRIDGE_TIMEOUT 28799

// Send buffer size
#define MAX_TXBUFFER 2048

enum connModes {
  cmInit = 0,        // initialization mode: nothing received yet
  cmTransparent,     // transparent mode
  cmAVR,             // Arduino/AVR programming mode
  cmARM,             // ARM (LPC8xx) programming
  cmEcho,            // simply echo characters (used for debugging latency)
  cmTelnet,          // use telnet escape sequences for programming mode
};

typedef struct serbridgeConnData {
  struct espconn *conn;
  enum connModes conn_mode;     // connection mode
  char           *txbuffer;     // buffer for the data to send
  uint16         txbufferlen;   // length of data in txbuffer
  char           *sentbuffer;   // buffer sent, awaiting callback to get freed
  bool           readytosend;   // true, if txbuffer can be sent by espconn_sent
  uint8_t        telnet_state;
} serbridgeConnData;

void ICACHE_FLASH_ATTR serbridgeInit(int port);
void ICACHE_FLASH_ATTR serbridgeInitPins(void);
void ICACHE_FLASH_ATTR serbridgeUartCb(char *buf, short len);
void ICACHE_FLASH_ATTR serbridgeReset();

#endif /* __SER_BRIDGE_H__ */
