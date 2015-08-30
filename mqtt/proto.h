#ifndef _PROTO_H_
#define	_PROTO_H_
#include <esp8266.h>
#include "ringbuf.h"

typedef void (PROTO_PARSE_CALLBACK)();

typedef struct {
  uint8_t* buf;
  uint16_t bufSize;
  uint16_t dataLen;
  uint8_t isEsc;
  uint8_t isBegin;
  PROTO_PARSE_CALLBACK* callback;
} PROTO_PARSER;

int8_t PROTO_Init(PROTO_PARSER* parser, PROTO_PARSE_CALLBACK* completeCallback, uint8_t* buf, uint16_t bufSize);
int16_t PROTO_AddRb(RINGBUF* rb, const uint8_t* packet, int16_t len);
int8_t PROTO_ParseByte(PROTO_PARSER* parser, uint8_t value);
int16_t PROTO_ParseRb(RINGBUF* rb, uint8_t* bufOut, uint16_t* len, uint16_t maxBufLen);
#endif
