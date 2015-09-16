#ifndef __SER_BRIDGE_H__
#define __SER_BRIDGE_H__

#include <ip_addr.h>
#include <c_types.h>
#include <espconn.h>

#define MAX_CONN 4
#define SER_BRIDGE_TIMEOUT 300 // 300 seconds = 5 minutes

// Send buffer size
#define MAX_TXBUFFER (2*1460)

typedef struct serbridgeConnData serbridgeConnData;

enum connModes {
	cmInit = 0,        // initialization mode: nothing received yet
	cmTransparent,     // transparent mode
	cmAVR,             // Arduino/AVR programming mode
	cmARM,             // ARM (LPC8xx) programming
	cmEcho,            // simply echo characters (used for debugging latency)
	cmCommand,         // AT command mode
  cmTelnet,          // use telnet escape sequences for programming mode
};

struct serbridgeConnData {
	struct espconn *conn;
	enum connModes conn_mode;     // connection mode
  uint8_t        telnet_state;
	uint16         txbufferlen;   // length of data in txbuffer
	char           *txbuffer;     // buffer for the data to send
  char           *sentbuffer;   // buffer sent, awaiting callback to get freed
  uint32_t       txoverflow_at; // when the transmitter started to overflow
	bool           readytosend;   // true, if txbuffer can be sent by espconn_sent
};

void ICACHE_FLASH_ATTR serbridgeInit(int port);
void ICACHE_FLASH_ATTR serbridgeInitPins(void);
void ICACHE_FLASH_ATTR serbridgeUartCb(char *buf, int len);
void ICACHE_FLASH_ATTR serbridgeReset();

#endif /* __SER_BRIDGE_H__ */
