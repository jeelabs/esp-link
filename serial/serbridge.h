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
  cmPGMInit,         // initialization mode for programming
  cmTransparent,     // transparent mode
  cmPGM,             // Arduino/AVR/ARM programming mode
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

// port1 is transparent&programming, second port is programming only
void ICACHE_FLASH_ATTR serbridgeInit(int port1, int port2);
void ICACHE_FLASH_ATTR serbridgeInitPins(void);
void ICACHE_FLASH_ATTR serbridgeUartCb(char *buf, short len);
void ICACHE_FLASH_ATTR serbridgeReset();

#endif /* __SER_BRIDGE_H__ */
