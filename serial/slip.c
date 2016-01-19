// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt

#include "esp8266.h"
#include "uart.h"
#include "crc16.h"
#include "serbridge.h"
#include "console.h"
#include "cmd.h"

#ifdef SLIP_DBG
#define DBG(format, ...) os_printf(format, ## __VA_ARGS__)
#else
#define DBG(format, ...) do { } while(0)
#endif

uint8_t slip_disabled;   // temporarily disable slip to allow flashing of attached MCU

extern void ICACHE_FLASH_ATTR console_process(char *buf, short len);

// This SLIP parser does not conform to RFC 1055 https://tools.ietf.org/html/rfc1055,
// instead, it implements the framing implemented in https://github.com/tuanpmt/esp_bridge
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
  if (slip_len < 1) return;

  if (!slip_inpkt) {
    // debug console stuff
    console_process(slip_buf, slip_len);
  } else {
    // proper SLIP packet, invoke command processor after checking CRC
    //os_printf("SLIP: rcv %d\n", slip_len);
    if (slip_len > 2) {
      uint16_t crc = crc16_data((uint8_t*)slip_buf, slip_len-2, 0);
      uint16_t rcv = ((uint16_t)slip_buf[slip_len-2]) | ((uint16_t)slip_buf[slip_len-1] << 8);
      if (crc == rcv) {
        CMD_parse_packet((uint8_t*)slip_buf, slip_len-2);
      } else {
        os_printf("SLIP: bad CRC, crc=%x rcv=%x\n", crc, rcv);

        for (short i=0; i<slip_len; i++) {
          if (slip_buf[i] >= ' ' && slip_buf[i] <= '~') {
            DBG("%c", slip_buf[i]);
          }
          else {
            DBG("\\%02X", slip_buf[i]);
          }
        }
        DBG("\n");
      }
    }
  }
}

#if 0
// determine whether a character is printable or not (or \r \n)
static bool ICACHE_FLASH_ATTR
slip_printable(char c) {
  return (c >= ' ' && c <= '~') || c == '\n' || c == '\r';
}
#endif

static void ICACHE_FLASH_ATTR
slip_reset() {
  //os_printf("SLIP: reset\n");
  slip_inpkt = false;
  slip_escaped = false;
  slip_len = 0;
}

// SLIP parse a single character
static void ICACHE_FLASH_ATTR
slip_parse_char(char c) {
  if (!slip_inpkt) {
    if (c == SLIP_START) {
      if (slip_len > 0) console_process(slip_buf, slip_len);
      slip_reset();
      slip_inpkt = true;
      DBG("SLIP: start\n");
      return;
    }
  } else if (slip_escaped) {
    // prev char was SLIP_REPL
    c = SLIP_ESC(c);
    slip_escaped = false;
  } else {
    switch (c) {
    case SLIP_REPL:
      slip_escaped = true;
      return;
    case SLIP_END:
      // end of packet, process it and get ready for next one
      if (slip_len > 0) slip_process();
      slip_reset();
      return;
    case SLIP_START:
      os_printf("SLIP: got SLIP_START while in packet?\n");
      //os_printf("SLIP: rcv %d:", slip_len);
      //for (int i=0; i<slip_len; i++) os_printf(" %02x", slip_buf[i]);
      //os_printf("\n");
      slip_reset();
      return;
    }
  }
  if (slip_len < SLIP_MAX) slip_buf[slip_len++] = c;
}

// callback with a buffer of characters that have arrived on the uart
void ICACHE_FLASH_ATTR
slip_parse_buf(char *buf, short length) {
  // do SLIP parsing
  for (short i=0; i<length; i++)
    slip_parse_char(buf[i]);

  // if we're in-between packets (debug console) then print it now
  if (!slip_inpkt && length > 0) {
    slip_process();
    slip_reset();
  }
}

