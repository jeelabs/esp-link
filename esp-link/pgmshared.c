// Copyright (c) 2015 by Thorsten von Eicken, see LICENSE.txt in the esp-link repo
// Copyright (c) 2016-2017 by Danny Backx

#include <esp8266.h>
#include <osapi.h>
#include "cgi.h"
#include "config.h"
#include "uart.h"
#include "stk500v2.h"
#include "serbridge.h"
#include "serled.h"

#include "pgmshared.h"

struct optibootData *optibootData;

char responseBuf[RESP_SZ]; // buffer to accumulate responses from optiboot
short responseLen = 0;     // amount accumulated so far
char errMessage[ERR_MAX];  // error message

// verify that N chars are hex characters
bool ICACHE_FLASH_ATTR checkHex(char *buf, short len) {
  while (len--) {
    char c = *buf++;
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))
      continue;
    DBG("OB non-hex\n");
    os_sprintf(errMessage, "Non hex char in POST record: '%c'/0x%02x", c, c);
    return false;
  }
  return true;
}

// get hex value of some hex characters
uint32_t ICACHE_FLASH_ATTR getHexValue(char *buf, short len) {
  uint32_t v = 0;
  while (len--) {
    v = (v<<4) | (uint32_t)(*buf & 0xf);
    if (*buf > '9') v += 9;
    buf++;
  }
  return v;
}

// verify checksum
static bool ICACHE_FLASH_ATTR verifyChecksum(char *buf, short len) {
  uint8_t sum = 0;
  while (len >= 2) {
    sum += (uint8_t)getHexValue(buf, 2);
    buf += 2;
    len -= 2;
  }
  return sum == 0;
}

// We've not built one function that works on both, but kept the different functions.
// This calls one or the other
static bool ICACHE_FLASH_ATTR programPage() {
  if (optibootData->mega)
    return megaProgramPage();
  return optibootProgramPage();
}

// Process a hex record -- assumes that the records starts with ':' & hex length
bool ICACHE_FLASH_ATTR processRecord(char *buf, short len) {
  buf++; len--; // skip leading ':'
  // check we have all hex chars
  if (!checkHex(buf, len)) return false;
  // verify checksum
  if (!verifyChecksum(buf, len)) {
    buf[len] = 0;
    os_sprintf(errMessage, "Invalid checksum for record %s", buf);
    return false;
  }
  // dispatch based on record type
  uint8_t type = getHexValue(buf+6, 2);
  switch (type) {
  case 0x00: { // Intel HEX data record
    //DBG("OB REC data %ld pglen=%d\n", getHexValue(buf, 2), optibootData->pageLen);
    uint32_t addr = getHexValue(buf+2, 4);
    // check whether we need to program previous record(s)
    if (optibootData->pageLen > 0 &&
        addr != ((optibootData->address+optibootData->pageLen)&0xffff)) {
      //DBG("OB addr chg\n");
      //DBG("processRecord addr chg, len %d, addr 0x%04x\n", optibootData->pageLen, addr);
      if (!programPage()) return false;
    }
    // set address, unless we're adding to the end (programPage may have changed pageLen)
    if (optibootData->pageLen == 0) {
      optibootData->address = (optibootData->address & 0xffff0000) | addr;
      //DBG("OB set-addr 0x%lx\n", optibootData->address);
    }
    // append record
    uint16_t recLen = getHexValue(buf, 2);
    for (uint16_t i=0; i<recLen; i++)
      optibootData->pageBuf[optibootData->pageLen++] = getHexValue(buf+8+2*i, 2);
    // program page, if we have a full page
    if (optibootData->pageLen >= optibootData->pgmSz) {
      //DBG("OB full\n");
      DBG("processRecord %d, call programPage() %08x\n", optibootData->pgmSz, optibootData->address + optibootData->segment);
      if (!programPage()) return false;
    }
    break; }
  case 0x01: // Intel HEX EOF record
    DBG("OB EOF\n");
    // program any remaining partial page
#if 1
    if (optibootData->pageLen > 0) {
      // DBG("processRecord remaining partial page, len %d, addr 0x%04x\n", optibootData->pageLen, optibootData->address + optibootData->segment);
      if (!programPage()) return false;
    }
    optibootData->eof = true;
#else
    if (optibootData->pageLen > 0) {
      // HACK : fill up with 0xFF
      while (optibootData->pageLen < 256) {
        optibootData->pageBuf[optibootData->pageLen++] = 0xFF;
      }
      if (!programPage()) return false;
    }
    optibootData->eof = true;
#endif
    break;
  case 0x04: // Intel HEX address record
    DBG("OB address 0x%x\n", getHexValue(buf+8, 4) << 16);
    // program any remaining partial page
    if (optibootData->pageLen > 0) {
      DBG("processRecord 0x04 remaining partial page, len %d, addr 0x%04x\n",
        optibootData->pageLen, optibootData->address + optibootData->segment);
      if (!programPage()) return false;
    }
    optibootData->address = getHexValue(buf+8, 4) << 16;
    break;
  case 0x05: // Intel HEX start address (MDK-ARM only)
    // ignore, there's no way to tell optiboot that...
    break;
  case 0x02: // Intel HEX extended segment address record
    // Depending on the case, just ignoring this record could solve the problem
    // optibootData->segment = getHexValue(buf+8, 4) << 4;
    DBG("OB segment 0x%08X\n", optibootData->segment);
    return true;
  default:
    // DBG("OB bad record type\n");
    DBG(errMessage, "Invalid/unknown record type: 0x%02x, packet %s", type, buf);
    return false;
  }
  return true;
}

