// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt

#ifndef PKTBUF_H
#define PKTBUF_H

typedef struct PktBuf {
  struct PktBuf *next;   // next buffer in chain
  uint16_t      filled;  // number of bytes filled in buffer
  uint8_t       data[0]; // data in buffer
} PktBuf;

// Allocate a new packet buffer of given length
PktBuf *PktBuf_New(uint16_t length);

// Append a buffer to the end of a packet buffer queue, returns new head
PktBuf *PktBuf_Push(PktBuf *headBuf, PktBuf *buf);

// Prepend a buffer to the beginning of a packet buffer queue, return new head
PktBuf * PktBuf_Unshift(PktBuf *headBuf, PktBuf *buf);

// Shift first buffer off queue, returns new head (not shifted buffer!)
PktBuf *PktBuf_Shift(PktBuf *headBuf);

// Shift first buffer off queue, free it, return new head
PktBuf *PktBuf_ShiftFree(PktBuf *headBuf);

#endif
