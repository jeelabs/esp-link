// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt

#include <esp8266.h>
#include "pktbuf.h"

#ifdef PKTBUF_DBG
//static void ICACHE_FLASH_ATTR
//PktBuf_Print(PktBuf *buf) {
//  os_printf("PktBuf:");
//  for (int i=-16; i<0; i++)
//    os_printf(" %02X", ((uint8_t*)buf)[i]);
//  os_printf(" %p", buf);
//  for (int i=0; i<16; i++)
//    os_printf(" %02X", ((uint8_t*)buf)[i]);
//  os_printf("\n");
//  os_printf("PktBuf: next=%p len=0x%04x\n",
//      ((void**)buf)[-4], ((uint16_t*)buf)[-6]);
//}
#endif


PktBuf * ICACHE_FLASH_ATTR
PktBuf_New(uint16_t length) {
  PktBuf *buf = os_zalloc(length+sizeof(PktBuf));
  buf->next = NULL;
  buf->filled = 0;
  //os_printf("PktBuf_New: %p l=%d->%d d=%p\n",
  //    buf, length, length+sizeof(PktBuf), buf->data);
  return buf;
}

PktBuf * ICACHE_FLASH_ATTR
PktBuf_Push(PktBuf *headBuf, PktBuf *buf) {
  if (headBuf == NULL) {
    //os_printf("PktBuf_Push: %p\n", buf);
    return buf;
  }
  PktBuf *h = headBuf;
  while (h->next != NULL) h = h->next;
  h->next = buf;
  //os_printf("PktBuf_Push: %p->..->%p\n", headBuf, buf);
  return headBuf;
}

PktBuf * ICACHE_FLASH_ATTR
PktBuf_Unshift(PktBuf *headBuf, PktBuf *buf) {
  buf->next = headBuf;
  //os_printf("PktBuf_Unshift: %p->%p\n", buf, buf->next);
  return buf;
}

PktBuf * ICACHE_FLASH_ATTR
PktBuf_Shift(PktBuf *headBuf) {
  PktBuf *buf = headBuf->next;
  headBuf->next = NULL;
  //os_printf("PktBuf_Shift: (%p)->%p\n", headBuf, buf);
  return buf;
}

PktBuf * ICACHE_FLASH_ATTR
PktBuf_ShiftFree(PktBuf *headBuf) {
  PktBuf *buf = headBuf->next;
  //os_printf("PktBuf_ShiftFree: (%p)->%p\n", headBuf, buf);
  os_free(headBuf);
  return buf;
}
