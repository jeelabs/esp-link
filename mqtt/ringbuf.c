#include "ringbuf.h"

/**
* \brief init a RINGBUF object
* \param r pointer to a RINGBUF object
* \param buf pointer to a byte array
* \param size size of buf
* \return 0 if successfull, otherwise failed
*/
int16_t ICACHE_FLASH_ATTR 
RINGBUF_Init(RINGBUF* r, uint8_t* buf, int32_t size) {
  if (r == NULL || buf == NULL || size < 2) return -1;

  r->p_o = r->p_r = r->p_w = buf;
  r->fill_cnt = 0;
  r->size = size;

  return 0;
}

/**
* \brief put a character into ring buffer
* \param r pointer to a ringbuf object
* \param c character to be put
* \return 0 if successfull, otherwise failed
*/
int16_t ICACHE_FLASH_ATTR 
RINGBUF_Put(RINGBUF* r, uint8_t c) {
  if (r->fill_cnt >= r->size)return -1; // ring buffer is full, this should be atomic operation


  r->fill_cnt++; // increase filled slots count, this should be atomic operation


  *r->p_w++ = c; // put character into buffer

  if (r->p_w >= r->p_o + r->size) // rollback if write pointer go pass
    r->p_w = r->p_o; // the physical boundary

  return 0;
}

/**
* \brief get a character from ring buffer
* \param r pointer to a ringbuf object
* \param c read character
* \return 0 if successfull, otherwise failed
*/
int16_t ICACHE_FLASH_ATTR 
RINGBUF_Get(RINGBUF* r, uint8_t* c) {
  if (r->fill_cnt <= 0)return -1; // ring buffer is empty, this should be atomic operation


  r->fill_cnt--; // decrease filled slots count


  *c = *r->p_r++; // get the character out

  if (r->p_r >= r->p_o + r->size) // rollback if write pointer go pass
    r->p_r = r->p_o; // the physical boundary

  return 0;
}
