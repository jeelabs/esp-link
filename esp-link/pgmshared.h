// Copyright (c) 2015 by Thorsten von Eicken, see LICENSE.txt in the esp-link repo
// Copyright (c) 2017 by Danny Backx

#ifndef _PGM_SHARED_H_
#define _PGM_SHARED_H_

#include "user_config.h"

#define RESP_SZ 64
#define ERR_MAX 128

extern char responseBuf[RESP_SZ];
extern short responseLen;
extern char errMessage[ERR_MAX];

// structure used to remember request details from one callback to the next
// allocated dynamically so we don't burn so much static RAM
extern struct optibootData {
  char *saved;               // buffer for saved incomplete hex records
  char *pageBuf;             // buffer for received data to be sent to AVR
  uint16_t pageLen;          // number of bytes in pageBuf
  uint16_t pgmSz;            // size of flash page to be programmed at a time
  uint32_t pgmDone;          // number of bytes programmed
  uint32_t address;          // address to write next page to
  uint32_t segment;          // for extended segment addressing, added to the address field
  uint32_t startTime;        // time of program POST request
  HttpdConnData *conn;       // request doing the programming, so we can cancel it
  bool eof;                  // got EOF record

  // Whether to use the Mega (STK500v2) protocol
  bool	mega;

  // STK500v2 variables
  int	hardwareVersion,
	firmwareVersionMajor,
	firmwareVersionMinor,
	vTarget;
  uint8_t signature[3];
  uint8_t	lfuse, hfuse, efuse;
} *optibootData;

bool ICACHE_FLASH_ATTR checkHex(char *buf, short len);
uint32_t ICACHE_FLASH_ATTR getHexValue(char *buf, short len);
bool ICACHE_FLASH_ATTR processRecord(char *buf, short len);
bool megaProgramPage(void);
bool optibootProgramPage(void);

#ifdef OPTIBOOT_DBG
#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)
#else
#define DBG(format, ...) do { } while(0)
#endif

#endif
