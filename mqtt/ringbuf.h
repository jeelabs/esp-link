#ifndef _RING_BUF_H_
#define _RING_BUF_H_

#include <esp8266.h>

typedef struct {
  uint8_t* p_o; /**< Original pointer */
  uint8_t* volatile p_r; /**< Read pointer */
  uint8_t* volatile p_w; /**< Write pointer */
  volatile int32_t fill_cnt; /**< Number of filled slots */
  int32_t size; /**< Buffer size */
} RINGBUF;

int16_t ICACHE_FLASH_ATTR RINGBUF_Init(RINGBUF* r, uint8_t* buf, int32_t size);
int16_t ICACHE_FLASH_ATTR RINGBUF_Put(RINGBUF* r, uint8_t c);
int16_t ICACHE_FLASH_ATTR RINGBUF_Get(RINGBUF* r, uint8_t* c);
#endif
