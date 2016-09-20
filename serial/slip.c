// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt

#include "esp8266.h"
#include "uart.h"
#include "crc16.h"
#include "serbridge.h"
#include "console.h"
#include "cmd.h"

#ifdef SLIP_DBG
#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)
#else
#define DBG(format, ...) do { } while(0)
#endif

extern void ICACHE_FLASH_ATTR console_process(char *buf, short len);

// This SLIP parser tries to conform to RFC 1055 https://tools.ietf.org/html/rfc1055.
// It accumulates each packet into a static buffer and calls cmd_parse() when the end
// of a packet is reached. It expects cmd_parse() to copy anything it needs from the
// buffer elsewhere as the buffer is immediately reused.
// One special feature is that if the first two characters of a packet are both printable or
// \n or \r then the parser assumes it's dealing with console debug output and calls
// slip_console(c) for each character and does not accumulate chars in the buffer until the
// next SLIP_END marker is seen. This allows random console debug output to come in between
// packets as long as each packet starts *and* ends with SLIP_END (which is an official
// variation on the SLIP protocol).

static bool slip_escaped;       // true when prev char received is escape
static bool slip_inpkt;         // true when we're after SLIP_START and before SLIP_END
#define SLIP_MAX 1024           // max length of SLIP packet
static char slip_buf[SLIP_MAX]; // buffer for current SLIP packet
static short slip_len;          // accumulated length in slip_buf

// SLIP process a packet or a bunch of debug console chars
static void ICACHE_FLASH_ATTR
slip_process() {
  if (slip_len > 2) {
    // proper SLIP packet, invoke command processor after checking CRC
    //os_printf("SLIP: rcv %d\n", slip_len);
    uint16_t crc = crc16_data((uint8_t*)slip_buf, slip_len-2, 0);
    uint16_t rcv = ((uint16_t)slip_buf[slip_len-2]) | ((uint16_t)slip_buf[slip_len-1] << 8);
    if (crc == rcv) {
      cmdParsePacket((uint8_t*)slip_buf, slip_len-2);
    } else {
      os_printf("SLIP: bad CRC, crc=%04x rcv=%04x len=%d\n", crc, rcv, slip_len);

      for (short i=0; i<slip_len; i++) {
        if (slip_buf[i] >= ' ' && slip_buf[i] <= '~') {
          DBG("%c", slip_buf[i]);
        } else {
          DBG("\\%02X", slip_buf[i]);
        }
      }
      DBG("\n");
    }
  }
}

// determine whether a character is printable or not (or \r \n)
static bool ICACHE_FLASH_ATTR
slip_printable(char c) {
  return (c >= ' ' && c <= '~') || c == '\n' || c == '\r';
}

static void ICACHE_FLASH_ATTR
slip_reset() {
  //os_printf("SLIP: reset\n");
  slip_inpkt = true;
  slip_escaped = false;
  slip_len = 0;
}

// SLIP parse a single character
static void ICACHE_FLASH_ATTR
slip_parse_char(char c) {
  if (c == SLIP_END) {
    // either start or end of packet, process whatever we may have accumulated
    DBG("SLIP: start or end len=%d inpkt=%d\n", slip_len, slip_inpkt);
    if (slip_len > 0) {
      if (slip_len > 2 && slip_inpkt) slip_process();
      else console_process(slip_buf, slip_len);
    }
    slip_reset();
  } else if (slip_escaped) {
    // prev char was SLIP_ESC
    if (c == SLIP_ESC_END) c = SLIP_END;
    if (c == SLIP_ESC_ESC) c = SLIP_ESC;
    if (slip_len < SLIP_MAX) slip_buf[slip_len++] = c;
    slip_escaped = false;
  } else if (slip_inpkt && c == SLIP_ESC) {
    slip_escaped = true;
  } else {
    if (slip_len == 1 && slip_printable(slip_buf[0]) && slip_printable(c)) {
      // start of packet and it's a printable character, we're gonna assume that this is console text
      slip_inpkt = false;
    }
    if (slip_len < SLIP_MAX) slip_buf[slip_len++] = c;
  }
}

// callback with a buffer of characters that have arrived on the uart
void ICACHE_FLASH_ATTR
slip_parse_buf(char *buf, short length) {
  // do SLIP parsing
  for (short i=0; i<length; i++)
    slip_parse_char(buf[i]);

  // if we're in-between packets (debug console) then print it now
  if (!slip_inpkt && length > 0) {
    console_process(slip_buf, slip_len);
    slip_len = 0;
  }
}

