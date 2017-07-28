/*
 * STK500v2 implementation for esp-link
 * Copyright (c) 2016-2017 by Danny Backx
 *
 * based on the framework for "optiboot" (STK500 v1 protocol) in esp-link
 * which is copyright (c) 2015 by Thorsten von Eicken
 *
 * see LICENSE.txt in the esp-link repo
 *
 * Documentation about the protocol used : see http://www.atmel.com/Images/doc2591.pdf
 *
 * Note the Intel HEX format is read by this code.
 * Format description is e.g. in http://www.keil.com/support/docs/1584/
 * Summary : each line (HEX file record) is formatted as ":llaaaatt[dd...]cc"
 *   : is the colon that starts every Intel HEX record.
 *   ll is the record-length field that represents the number of data bytes (dd) in the record.
 *   aaaa is the address field that represents the starting address for subsequent data.
 *   tt is the field that represents the HEX record type, which may be one of the following:
 *      00 - data record
 *      01 - end-of-file record
 *      02 - extended segment address record
 *      04 - extended linear address record
 *      05 - start linear address record (MDK-ARM only)
 *   dd is a data field that represents one byte of data. A record may have multiple data bytes.
 *     The number of data bytes in the record must match the number specified by the ll field.
 *   cc is the checksum field that represents the checksum of the record. The checksum is
 *     calculated by summing the values of all hexadecimal digit pairs in the record modulo
 *     256 and taking the two's complement.
 *
 */

#include <esp8266.h>
#include <osapi.h>
#include "cgi.h"
#include "config.h"
#include "uart.h"
#include "stk500v2.h"
#include "serbridge.h"
#include "serled.h"
#include "pgmshared.h"

#define INIT_DELAY     150   // wait this many millisecs before sending anything
#define BAUD_INTERVAL  600   // interval after which we change baud rate
#define PGM_TIMEOUT  20000   // timeout after sync is achieved, in milliseconds
#define PGM_INTERVAL   200   // send sync at this interval in ms when in programming mode
#define ATTEMPTS         8   // number of attempts total to make

#define DBG_GPIO5 1 // define to 1 to use GPIO5 to trigger scope

//===== global state

static ETSTimer optibootTimer;

static enum {		// overall programming states
  stateInit = 0,	// initial delay
  stateSync,		// waiting to hear back
  stateVar1,		// Steps in reading subsequent parameters
  stateVar2,
  stateVar3,
  stateGetSig1,		// Reading device signature
  stateGetSig2,
  stateGetSig3,
  stateGetFuse1,
  stateGetFuse2,
  stateGetFuse3,
  stateProg,		// programming...
} progState;
static char* progStates[] = { "init", "sync", "var1", "var2", "var3",
	"sig1", "sig2", "sig3",
	"fuse1", "fuse2", "fuse3",
	"prog" };
static short baudCnt;        // counter for sync attempts at different baud rates
static short ackWait;        // counter of expected ACKs
static uint32_t baudRate;    // baud rate at which we're programming

#define MAX_PAGE_SZ 512      // max flash page size supported
#define MAX_SAVED   512      // max chars in saved buffer

// Sequence number of current command/answer. Increment before sending packet.
static int cur_seqno;

// Structure of a STK500v2 message
struct packet {
  uint8_t start;	// 0x1B
  uint8_t seqno;
  uint8_t ms1, ms2;
  uint8_t token;	// 0x0E
  uint8_t *body;	// documented max length is 275
  uint8_t cksum;	// xor of all bytes including start and body
} pbuf;

// forward function references
static void megaTimerCB(void *);
static void megaUartRecv(char *buffer, short length);
static void armTimer(uint32_t ms);
static void initBaud(void);
static void initPacket();
static void allocateOptibootData();
#if 0
static void cleanupPacket();
#endif
static int readSyncPacket();
static void sendSyncPacket();
static void sendQueryPacket(int param);
static void sendQuerySignaturePacket(int param);
static void sendRebootMCUQuery();
static void readRebootMCUReply();
static void sendLoadAddressQuery(uint32_t);
static void sendProgramPageQuery(char *, int);
static void readProgramPageReply();
static bool reply_ok = false;
static int readReadFuseReply();
static int getFuseReply(char *, int);
static void sendReadFuseQuery(char fuse);

static void debug_reset();
static bool debug();
static int debug_cnt = 0;
static void debugOn(bool on);

static void ICACHE_FLASH_ATTR optibootInit() {
  debug_reset();

  progState = stateInit;
  baudCnt = 0;
  uart0_baud(flashConfig.baud_rate);
  ackWait = 0;
  errMessage[0] = 0;
  responseLen = 0;
  programmingCB = NULL;
  if (optibootData != NULL) {
    if (optibootData->conn != NULL)
      optibootData->conn->cgiPrivData = (void *)-1; // signal that request has been aborted
    if (optibootData->pageBuf) os_free(optibootData->pageBuf);
    if (optibootData->saved) os_free(optibootData->saved);
    os_free(optibootData);
    optibootData = NULL;
  }
  os_timer_disarm(&optibootTimer);
  DBG("OB init\n");

  reply_ok = false;
}

// append one string to another but visually escape non-printing characters in the second
// string using \x00 hex notation, max is the max chars in the concatenated string.
static void ICACHE_FLASH_ATTR appendPretty(char *buf, int max, char *raw, int rawLen) {
  int off = strlen(buf);
  max -= off + 1; // for null termination
  for (int i=0; i<max && i<rawLen; i++) {
    unsigned char c = raw[i++];
    if (c >= ' ' && c <= '~') {
      buf[off++] = c;
    } else if (c == '\n') {
      buf[off++] = '\\';
      buf[off++] = 'n';
    } else if (c == '\r') {
      buf[off++] = '\\';
      buf[off++] = 'r';
    } else {
      buf[off++] = '\\';
      buf[off++] = 'x';
      buf[off++] = '0'+(unsigned char)((c>>4)+((c>>4)>9?7:0));
      buf[off++] = '0'+(unsigned char)((c&0xf)+((c&0xf)>9?7:0));
    }
  }
  buf[off] = 0;
}

//===== Cgi to reset AVR and get Optiboot into sync
int ICACHE_FLASH_ATTR cgiMegaSync(HttpdConnData *connData) {
  if (connData->conn==NULL)
    return HTTPD_CGI_DONE;	// Connection aborted. Clean up.

  // check that we know the reset pin, else error out with that
  if (flashConfig.reset_pin < 0) {
    errorResponse(connData, 400, "No reset pin defined");

  } else if (connData->requestType == HTTPD_METHOD_POST) {
    if (debug()) DBG("cgiMegaSync POST\n");

    // issue reset
    optibootInit();
    baudRate = flashConfig.baud_rate;
    programmingCB = megaUartRecv;
    initBaud();
    serbridgeReset();
#if DBG_GPIO5
    makeGpio(5);
    gpio_output_set(0, (1<<5), (1<<5), 0); // output 0
#endif

    // start sync timer
    os_timer_disarm(&optibootTimer);
    os_timer_setfn(&optibootTimer, megaTimerCB, NULL);
    os_timer_arm(&optibootTimer, INIT_DELAY, 0);

    // respond with optimistic OK
    noCacheHeaders(connData, 204);
    httpdEndHeaders(connData);
    httpdSend(connData, "", 0);

  } else if (connData->requestType == HTTPD_METHOD_GET) {
    if (debug()) DBG("cgiMegaSync GET\n");

    noCacheHeaders(connData, 200);
    httpdEndHeaders(connData);
    if (!errMessage[0] && progState >= stateProg) {
      char buf[64];
      if (optibootData == NULL)
	allocateOptibootData();
      DBG("OB got sync\n");
      os_sprintf(buf, "SYNC at %d baud, board %02x.%02x.%02x, hardware v%d, firmware %d.%d",
	baudRate, optibootData->signature[0], optibootData->signature[1], optibootData->signature[2],
	optibootData->hardwareVersion, optibootData->firmwareVersionMajor, optibootData->firmwareVersionMinor);
      httpdSend(connData, buf, -1);
    } else if (errMessage[0] && progState == stateSync) {
      DBG("OB cannot sync\n");
      char buf[512];
      os_sprintf(buf, "FAILED to SYNC: %s, got: %d chars\r\n", errMessage, responseLen);
      appendPretty(buf, 512, responseBuf, responseLen);
      httpdSend(connData, buf, -1);

      // All done, cleanup
      // cleanupPacket();
    } else {
      httpdSend(connData, errMessage[0] ? errMessage : "NOT READY", -1);
    }

  } else {
    errorResponse(connData, 404, "Only GET and POST supported");
  }

  return HTTPD_CGI_DONE;
}

// Allocate and initialize, to be called upon first CGI query
static void ICACHE_FLASH_ATTR initPacket() {
  cur_seqno = 0;

  pbuf.body = os_malloc(275);
  pbuf.start = MESSAGE_START;	// 0x1B
  pbuf.token = TOKEN;		// 0x0E
}

static void ICACHE_FLASH_ATTR writePacket() {
  int i, len;
  len = (pbuf.ms1 << 8) + pbuf.ms2;

  // Copy sequence number after incrementing it.
  pbuf.seqno = ++cur_seqno;

  // Compute cksum
  pbuf.cksum = 0;
  pbuf.cksum ^= pbuf.start;
  pbuf.cksum ^= pbuf.seqno;
  pbuf.cksum ^= pbuf.ms1;
  pbuf.cksum ^= pbuf.ms2;
  pbuf.cksum ^= pbuf.token;
  for (i=0; i<len; i++)
    pbuf.cksum ^= pbuf.body[i];

  uart0_write_char(pbuf.start);
  uart0_write_char(pbuf.seqno);
  uart0_write_char(pbuf.ms1);
  uart0_write_char(pbuf.ms2);
  uart0_write_char(pbuf.token);
  for (i=0; i<len; i++)
    uart0_write_char(pbuf.body[i]);
  uart0_write_char(pbuf.cksum);

#if 1
  if (debug()) {
    DBG("Packet sent : %02x %02x %02x %02x %02x - ",
      pbuf.start, pbuf.seqno, pbuf.ms1, pbuf.ms2, pbuf.token);
    for (i=0; i<len; i++)
      DBG(" %02x", pbuf.body[i]);
    DBG(" - %02x\n", pbuf.cksum);
  }
#else
    // write a limited amount of the packet
    DBG("Packet sent : %02x %02x %02x %02x %02x - ",
      pbuf.start, pbuf.seqno, pbuf.ms1, pbuf.ms2, pbuf.token);
    for (i=0; i<len && i<16; i++)
      DBG(" %02x", pbuf.body[i]);
    DBG(" ...\n");
#endif
}

static void ICACHE_FLASH_ATTR sendSyncPacket() {
  pbuf.ms1 = 0x00;
  pbuf.ms2 = 0x01;
  pbuf.body[0] = CMD_SIGN_ON;

  writePacket();
}

static void ICACHE_FLASH_ATTR sendQueryPacket(int param) {
  pbuf.ms1 = 0x00;
  pbuf.ms2 = 0x02;
  pbuf.body[0] = CMD_GET_PARAMETER;
  pbuf.body[1] = param;

  writePacket();
}

static void ICACHE_FLASH_ATTR sendQuerySignaturePacket(int param) {
  pbuf.ms1 = 0;
  pbuf.ms2 = 8;
  pbuf.body[0] = CMD_SPI_MULTI;
  pbuf.body[1] = 4;
  pbuf.body[2] = 4;
  pbuf.body[3] = 0;
  pbuf.body[4] = 0x30;
  pbuf.body[5] = 0;
  pbuf.body[6] = param;
  pbuf.body[7] = 0;

  writePacket();
}

/*
 * Read the UART directly
 *
 * Returns the DATA length, not the packet length
 */
static int ICACHE_FLASH_ATTR readPacket() {
  char recv1[8], recv2[4];
  int i;

  uint16_t got1 = uart0_rx_poll(recv1, 5, 50000);	// 5 : start + seqno + 2 x size + token
  if (got1 < 5) {
    DBG("readPacket: got1 %d, expected 5\n", got1);
    return -1;
  }
  uint16_t len = (recv1[2] << 8) + recv1[3];
  uint16_t got2 = uart0_rx_poll((char *)pbuf.body, len, 50000);
  if (got2 < len) {
    DBG("readPacket: got2 %d, expected %d\n", got2, len);
    return -1;
  }
  uint16_t got3 = uart0_rx_poll(recv2, 1, 50000);
  if (got3 < 1) {
    DBG("readPacket: got3 %d, expected 1\n", got3);
    return -1;
  }
#if 1
  if (debug()) {
    DBG("Packet received : ");
    for (i=0; i<5; i++)
      DBG(" %02x", recv1[i]);
    DBG(" - ");
    for (i=0; i<len && i<80; i++)
      DBG(" %02x", pbuf.body[i]);
    DBG(" -  %02x\n", recv2[0]);
  }
#endif
  if (recv1[0] == MESSAGE_START && recv1[1] == cur_seqno && recv1[4] == TOKEN) {
    // DBG("OB valid packet received, len %d\n", len);
  }

  // Compute cksum
  uint8_t x = 0;
  x = 0;
  for (i=0; i<5; i++)
    x ^= recv1[i];
  for (i=0; i<len; i++)
    x ^= pbuf.body[i];

  if (x != recv2[0]) {
    DBG("Invalid checksum received, %02x expected %02x, ignoring packet.\n", x, recv2[0]);

    // Return a negative value if the checksum is bad, but don't lose the byte count.
    return -len;
  }

  return len;
}

static int ICACHE_FLASH_ATTR readSyncPacket() {
  int len = readPacket();
  if (len == 11) {
    if (pbuf.body[0] == CMD_SIGN_ON && pbuf.body[1] == STATUS_CMD_OK && pbuf.body[2] == 8) {
      pbuf.body[len] = 0;
      if (debug()) DBG("Device identifies as %s\n", pbuf.body+3);
      return len;
    } else {
      DBG("Invalid packet\n");
      return len;
    }
  } else if (len < 0) {
    return len;	// This is bad but already reported this, so just pass the info.
  } else {
    DBG("Sync reply length invalid : %d should be 11\n", len);
    return len;
  }
}

static void ICACHE_FLASH_ATTR sendRebootMCUQuery() {
  pbuf.ms1 = 0;
  pbuf.ms2 = 3;
  pbuf.body[0] = CMD_LEAVE_PROGMODE_ISP;
  pbuf.body[1] = 1;
  pbuf.body[2] = 1;

  writePacket();
}

static void ICACHE_FLASH_ATTR readRebootMCUReply() {
  int len = readPacket();
  if (len == 2) {
    if (pbuf.body[0] == CMD_LEAVE_PROGMODE_ISP && pbuf.body[1] == STATUS_CMD_OK) {
      reply_ok = true;
      if (debug()) DBG("Rebooting MCU : ok\n");
      return;
    }
  }
}

static void ICACHE_FLASH_ATTR sendReadFlashQuery() {
  pbuf.ms1 = 0;
  pbuf.ms2 = 4;
  pbuf.body[0] = CMD_READ_FLASH_ISP;
  pbuf.body[1] = 0;	// Number of bytes to read, MSB first
  pbuf.body[2] = 16;
  pbuf.body[3] = 0x20;	// ??

  writePacket();
}

static void ICACHE_FLASH_ATTR readReadFlashReply(char *buf) {
  int len = readPacket();
  if (len == 19) {
    if (pbuf.body[0] == CMD_READ_FLASH_ISP && pbuf.body[1] == STATUS_CMD_OK) {
      int i;
      for (i=0; i<len-3; i++)
        buf[i] = pbuf.body[i+2];
    }
    reply_ok = true;
  } else
    reply_ok = false;
}

static void ICACHE_FLASH_ATTR sendLoadAddressQuery(uint32_t addr) {
  pbuf.ms1 = 0;
  pbuf.ms2 = 5;

  // Divide address by 2, to cope with addressing words, not bytes
  int a2 = addr >> 1;

  if (pbuf.body == 0)
    return;
  pbuf.body[0] = CMD_LOAD_ADDRESS;
  pbuf.body[1] = 0xFF & (a2 >> 24);
  pbuf.body[2] = 0xFF & (a2 >> 16);
  pbuf.body[3] = 0xFF & (a2 >> 8);
  pbuf.body[4] = 0xFF & a2;

  writePacket();
}

static void ICACHE_FLASH_ATTR readLoadAddressReply() {
  reply_ok = false;

  int len = readPacket();
  if (len == 2) {
    if (pbuf.body[0] == CMD_LOAD_ADDRESS && pbuf.body[1] == STATUS_CMD_OK) {
      reply_ok = true;
      // DBG("LoadAddress Reply : ok\n");
      return;
    }
  }

  DBG("LoadAddress : %02x %02x (expected %02x %02x), len %d exp 2\n",
    pbuf.body[0], pbuf.body[1], CMD_LOAD_ADDRESS, STATUS_CMD_OK, len);
}

static void ICACHE_FLASH_ATTR sendProgramPageQuery(char *data, int pgmLen) {
  int totalLen = pgmLen + 10;
  int mode = 0xC1;		// already includes the 0x80 "write page" bit

  pbuf.ms1 = totalLen >> 8;
  pbuf.ms2 = totalLen & 0xFF;

  pbuf.body[0] = CMD_PROGRAM_FLASH_ISP;
  pbuf.body[1] = pgmLen >> 8;
  pbuf.body[2] = pgmLen & 0xFF;
  pbuf.body[3] = mode;		// mode byte
  pbuf.body[4] = 0x0A;		// delay
  pbuf.body[5] = 0x40;		// cmd1
  pbuf.body[6] = 0x4C;		// cmd2
  pbuf.body[7] = 0x20;		// cmd3
  pbuf.body[8] = 0x00;		// poll1
  pbuf.body[9] = 0x00;		// poll2
  for (int i=0, j=10; i<pgmLen; i++, j++)
    pbuf.body[j] = data[i];

      // os_delay_us(4500L);	// Flashing takes about 4.5ms
  writePacket();
      // os_delay_us(4500L);	// Flashing takes about 4.5ms
}

static void ICACHE_FLASH_ATTR readProgramPageReply() {
  int len = readPacket();
  if (len == 2) {
    if (pbuf.body[0] == CMD_PROGRAM_FLASH_ISP && pbuf.body[1] == STATUS_CMD_OK) {
      reply_ok = true;
      // DBG("Program page : ok\n");
      // os_delay_us(4500L);	// Flashing takes about 4.5ms
      return;
    }
  }
}

static void ICACHE_FLASH_ATTR allocateOptibootData() {
  optibootData = os_zalloc(sizeof(struct optibootData));
  char *saved = os_zalloc(MAX_SAVED+1); // need space for string terminator
  char *pageBuf = os_zalloc(MAX_PAGE_SZ+MAX_SAVED/2);
  if (!optibootData || !pageBuf || !saved) {
    return;
  }
  optibootData->pageBuf = pageBuf;
  optibootData->saved = saved;
  optibootData->startTime = system_get_time();
  optibootData->mega = true;
  optibootData->pgmSz = 256;			// Try to force this to write 256 bytes per page
  DBG("OB data alloc\n");

  optibootData->address = optibootData->segment;
}

/*
 * This is called on /pgmmega/upload .
 *
 * Functionality provided : flash the hex file that is sent to the Arduino Mega.
 * Prerequisite : a succesfull call to /pgmmega/sync to force the Mega into programming mode
 *	and set up synchronized communication with it.
 */
int ICACHE_FLASH_ATTR cgiMegaData(HttpdConnData *connData) {
  // DBG("cgiMegaData state=%d postLen=%d\n", progState, connData->post->len);

  if (connData->conn==NULL)
    return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  if (!optibootData)
    if (debug()) DBG("OB pgm: state=%d postLen=%d\n", progState, connData->post->len);

  // check that we have sync
  if (errMessage[0] || progState < stateProg) {
    DBG("OB not in sync, state=%d (%s), err=%s\n", progState, progStates[progState], errMessage);
    errorResponse(connData, 400, errMessage[0] ? errMessage : "Optiboot not in sync");
    return HTTPD_CGI_DONE;
  }

  // check that we don't have two concurrent programming requests going on
  if (connData->cgiPrivData == (void *)-1) {
    DBG("OB aborted\n");
    errorResponse(connData, 400, "Request got aborted by a concurrent sync request");
    return HTTPD_CGI_DONE;
  }

  // allocate data structure to track programming
  if (!optibootData) {
    allocateOptibootData();
    if (!optibootData || !optibootData->pageBuf || !optibootData->saved) {
      errorResponse(connData, 400, "Out of memory");
      return HTTPD_CGI_DONE;
    }
  }

  // iterate through the data received and program the AVR one block at a time
  HttpdPostData *post = connData->post;
  char *saved = optibootData->saved;
  while (post->buffLen > 0) {
    // first fill-up the saved buffer
    short saveLen = strlen(saved);
    if (saveLen < MAX_SAVED) {
      short cpy = MAX_SAVED-saveLen;
      if (cpy > post->buffLen) cpy = post->buffLen;
      os_memcpy(saved+saveLen, post->buff, cpy);
      saveLen += cpy;
      saved[saveLen] = 0; // string terminator
      os_memmove(post->buff, post->buff+cpy, post->buffLen-cpy);
      post->buffLen -= cpy;
      //DBG("OB cp %d buff->saved\n", cpy);
    }

    // process HEX records
    while (saveLen >= 11) { // 11 is minimal record length
      // skip any CR/LF
      short skip = 0;
      while (skip < saveLen && (saved[skip] == '\n' || saved[skip] == '\r'))
        skip++;
      if (skip > 0) {
        // shift out cr/lf (keep terminating \0)
        os_memmove(saved, saved+skip, saveLen+1-skip);
        saveLen -= skip;
        if (saveLen < 11) break;
        DBG("OB skip %d cr/lf\n", skip);
      }

      // inspect whether we have a proper record start
      if (saved[0] != ':') {
        DBG("OB found non-: start\n");
        os_sprintf(errMessage, "Expected start of record in POST data, got %s", saved);
        errorResponse(connData, 400, errMessage);
        optibootInit();
        return HTTPD_CGI_DONE;
      }

      if (!checkHex(saved+1, 2)) {
        errorResponse(connData, 400, errMessage);
        optibootInit();
        return HTTPD_CGI_DONE;
      }
      uint8_t recLen = getHexValue(saved+1, 2);
      //DBG("OB record %d\n", recLen);

      // process record
      if (saveLen >= 11+recLen*2) {
        if (!processRecord(saved, 11+recLen*2)) {
          DBG("OB process err %s\n", errMessage);
          errorResponse(connData, 400, errMessage);
          optibootInit();
          return HTTPD_CGI_DONE;
        }
        short shift = 11+recLen*2;
        os_memmove(saved, saved+shift, saveLen+1-shift);
        saveLen -= shift;
        //DBG("OB %d byte record\n", shift);
      } else {
        break;
      }
    }
  }

  short code;
  if (post->received < post->len) {
    //DBG("OB pgm need more\n");
    return HTTPD_CGI_MORE;
  }

  if (optibootData->eof) {
    code = 200;
    // calculate some stats
    float dt = ((system_get_time() - optibootData->startTime)/1000)/1000.0; // in seconds
    uint32_t pgmDone = optibootData->pgmDone;
    optibootInit();
    os_sprintf(errMessage, "Success. %d bytes at %d baud in %d.%ds, %dB/s %d%% efficient",
        pgmDone, baudRate, (int)dt, (int)(dt*10)%10, (int)(pgmDone/dt),
        (int)(100.0*(10.0*pgmDone/baudRate)/dt));
  } else {
    code = 400;
    optibootInit();
    os_strcpy(errMessage, "Improperly terminated POST data");
  }

  DBG("OB pgm done: %d -- %s\n", code, errMessage);
  noCacheHeaders(connData, code);
  httpdEndHeaders(connData);
  httpdSend(connData, errMessage, -1);
  errMessage[0] = 0;

  return HTTPD_CGI_DONE;
}

// Program a flash page
bool ICACHE_FLASH_ATTR megaProgramPage(void) {
  if (optibootData == NULL)
    allocateOptibootData();
  if (debug())
    DBG("programPage len %d addr 0x%04x\n", optibootData->pageLen, optibootData->address + optibootData->segment);

  if (optibootData->pageLen == 0)
    return true;
  armTimer(PGM_TIMEOUT); // keep the timerCB out of the picture

  uint16_t pgmLen = optibootData->pageLen;
  if (pgmLen > optibootData->pgmSz)
    pgmLen = optibootData->pgmSz;
  // DBG("OB pgm %d@0x%x\n", pgmLen, optibootData->address + optibootData->segment);

  // send address to optiboot (little endian format)
#ifdef DBG_GPIO5
  gpio_output_set((1<<5), 0, (1<<5), 0); // output 1
#endif
  ackWait++;

  uint32_t addr = optibootData->address + optibootData->segment;
  sendLoadAddressQuery(addr);
  readLoadAddressReply();

  armTimer(PGM_TIMEOUT);
  if (! reply_ok) {
    DBG("OB pgm failed in load address\n");
    return false;
  }
  armTimer(PGM_TIMEOUT);
  // DBG("OB sent address 0x%04x\n", addr);

  // send page content
  sendProgramPageQuery(optibootData->pageBuf, pgmLen);

  armTimer(PGM_TIMEOUT);
  readProgramPageReply();
  armTimer(PGM_TIMEOUT);
  if (!reply_ok) {
    DBG("OB pgm failed in prog page\n");
    return false;
  }

  // shift data out of buffer
  os_memmove(optibootData->pageBuf, optibootData->pageBuf+pgmLen, optibootData->pageLen-pgmLen);
  optibootData->pageLen -= pgmLen;
#if 1
  optibootData->address += pgmLen;
#else
  DBG("Address old %08x ", optibootData->address + optibootData->segment);
  optibootData->address += pgmLen;
  DBG(" new %08x\n", optibootData->address + optibootData->segment);
#endif
  optibootData->pgmDone += pgmLen;

  // DBG("OB pgm OK\n");
  return true;
}

//===== Rebooting and getting sync

static void ICACHE_FLASH_ATTR armTimer(uint32_t ms) {
  os_timer_disarm(&optibootTimer);
  os_timer_arm(&optibootTimer, ms, 0);
}

static int baudRates[] = { 0, 9600, 57600, 115200 };

static void ICACHE_FLASH_ATTR setBaud() {
  baudRate = baudRates[(baudCnt++) % 4];
  uart0_baud(baudRate);
  //DBG("OB changing to %ld baud\n", baudRate);
}

static void ICACHE_FLASH_ATTR initBaud() {
  baudRates[0] = flashConfig.baud_rate;
  setBaud();
}

/*
 * The timer callback does two things :
 * 1. initially we await a timeout to start chatting with the Mega. This is good.
 * 2. in other cases, timeouts are a problem, we failed somewhere.
 */
static void ICACHE_FLASH_ATTR megaTimerCB(void *arg) {
  if (debug()) DBG("megaTimerCB state %d (%s)\n", progState, progStates[progState]);

  switch (progState) {
    case stateInit: // initial delay expired, send sync chars
      initPacket();

      // DBG("Reset pin %d to LOW ...", flashConfig.reset_pin);
      GPIO_OUTPUT_SET(flashConfig.reset_pin, 0);
      os_delay_us(2000L);	// Keep reset line low for 2 ms
      GPIO_OUTPUT_SET(flashConfig.reset_pin, 1);
      // DBG(" and up again.\n");

      os_delay_us(2000L);	// Now wait an additional 2 ms before sending packets

      // Send a couple of packets, until we get a reply
      int ok = -1, i = 0;
      while (ok < 0 && i++ < 8) {
        os_delay_us(200L);
	sendSyncPacket();
	cur_seqno++;
	ok = readSyncPacket();
      }
      if (ok < 0)
        break;		// Don't increment progState

      progState++;

      // Discard input until now
      responseLen = 0;

      // Send a query, reply to be picked up by megaUartRecv().
      sendQueryPacket(PARAM_HW_VER);

      // Trigger timeout
      armTimer(BAUD_INTERVAL-INIT_DELAY);
      return;
    case stateSync: // oops, must have not heard back!?
    case stateProg: // we're programming and we timed-out of inaction
    default: // we're trying to get some info from optiboot and it should have responded!
      break;
  }
}

/*
 * Receive response from optiboot.
 *
 * Based on our state, pick the response and interpret accordingly; then move state forward
 * while sending the next query.
 *
 * This plays a ping-pong game between us and the MCU.
 * Initial query is sent from megaTimerCB().
 */
static void ICACHE_FLASH_ATTR megaUartRecv(char *buf, short length) {
  int ok;

  //if (progState > stateGetSig3)
    if (debug()) {
      DBG("megaUartRecv %d bytes, ", length);
      for (int i=0; i<length; i++)
        DBG(" %02x", buf[i]);
      DBG("\n");
    }

  // append what we got to what we have accumulated
  if (responseLen < RESP_SZ-1) {
    char *rb = responseBuf+responseLen;
    for (short i=0; i<length && (rb-responseBuf)<(RESP_SZ-1); i++)
      *rb++ = buf[i];
    responseLen = rb-responseBuf;
    responseBuf[responseLen] = 0; // string terminator
  }

  // dispatch based on the current state
  switch (progState) {
  case stateInit:  // we haven't sent anything, this must be garbage
    break;
  case stateSync: // we're trying to get a sync response
    ok = 0;
    if (optibootData == NULL) {
      allocateOptibootData();
      DBG("megaUartRecv NULL stateSync\n");
      // break;
    }
    if (responseLen >= 9) {
      if (responseBuf[4] == TOKEN && responseBuf[5] == CMD_GET_PARAMETER && responseBuf[6] == STATUS_CMD_OK) {
        optibootData->hardwareVersion = responseBuf[7];
	ok++;
      }
      os_memcpy(responseBuf, responseBuf+9, responseLen-9);
      responseLen -= 9;
    }
    progState++;
    sendQueryPacket(PARAM_SW_MAJOR);
    armTimer(PGM_INTERVAL); // reset timer
    break;
  case stateVar1:
    if (responseLen >= 9) {
      if (responseBuf[4] == TOKEN && responseBuf[5] == CMD_GET_PARAMETER && responseBuf[6] == STATUS_CMD_OK) {
        optibootData->firmwareVersionMajor = responseBuf[7];
	ok++;
      }
      os_memcpy(responseBuf, responseBuf+9, responseLen-9);
      responseLen -= 9;
    }
    progState++;
    sendQueryPacket(PARAM_SW_MINOR);
    armTimer(PGM_INTERVAL); // reset timer
    break;
  case stateVar2:
    if (responseLen >= 9) {
      if (responseBuf[4] == TOKEN && responseBuf[5] == CMD_GET_PARAMETER && responseBuf[6] == STATUS_CMD_OK) {
        optibootData->firmwareVersionMinor = responseBuf[7];
	ok++;
      }
      os_memcpy(responseBuf, responseBuf+9, responseLen-9);
      responseLen -= 9;
    }
    progState++;
    sendQueryPacket(PARAM_VTARGET);
    armTimer(PGM_INTERVAL); // reset timer
    break;
  case stateVar3:
    if (responseLen >= 9) {
      if (responseBuf[4] == TOKEN && responseBuf[5] == CMD_GET_PARAMETER && responseBuf[6] == STATUS_CMD_OK) {
        optibootData->vTarget = responseBuf[7];
	ok++;
      }
      os_memcpy(responseBuf, responseBuf+9, responseLen-9);
      responseLen -= 9;
    }

    progState++;
    sendQuerySignaturePacket(0);
    if (debug())
      DBG("Hardware version %d, firmware %d.%d. Vtarget = %d.%d V\n",
        optibootData->hardwareVersion, optibootData->firmwareVersionMajor, optibootData->firmwareVersionMinor, optibootData->vTarget / 16, optibootData->vTarget % 16);
    armTimer(PGM_INTERVAL); // reset timer
    break;
  case stateGetSig1:
    if (responseLen >= 13) {
      if (responseBuf[4] == TOKEN && responseBuf[5] == CMD_SPI_MULTI && responseBuf[3] == 7) {
        optibootData->signature[0] = responseBuf[10];
      }
      os_memcpy(responseBuf, responseBuf+13, responseLen-13);
      responseLen -= 13;
      progState++;
      sendQuerySignaturePacket(1);
    }
    armTimer(PGM_INTERVAL); // reset timer
    break;
  case stateGetSig2:
    if (responseLen >= 13) {
      if (responseBuf[4] == TOKEN && responseBuf[5] == CMD_SPI_MULTI && responseBuf[3] == 7) {
        optibootData->signature[1] = responseBuf[10];
      }
      os_memcpy(responseBuf, responseBuf+13, responseLen-13);
      responseLen -= 13;
      progState++;
      sendQuerySignaturePacket(2);
    }
    armTimer(PGM_INTERVAL); // reset timer
    break;
  case stateGetSig3:
    if (responseLen >= 13) {
      if (responseBuf[4] == TOKEN && responseBuf[5] == CMD_SPI_MULTI && responseBuf[3] == 7) {
        optibootData->signature[2] = responseBuf[10];
      }
      os_memcpy(responseBuf, responseBuf+13, responseLen-13);
      responseLen -= 13;
      progState++;

      if (debug()) DBG("Board signature %02x.%02x.%02x\n", optibootData->signature[0], optibootData->signature[1], optibootData->signature[2]);
      sendReadFuseQuery('l');
    }
    armTimer(PGM_INTERVAL); // reset timer
    break;

  case stateGetFuse1:
    optibootData->lfuse = getFuseReply(buf, length);
    sendReadFuseQuery('h');
    progState++;
    break;

  case stateGetFuse2:
    optibootData->hfuse = getFuseReply(buf, length);
    sendReadFuseQuery('e');
    progState++;
    break;

  case stateGetFuse3:
    optibootData->efuse = getFuseReply(buf, length);
    if (debug())
      DBG("Fuses %02x %02x %02x\n", optibootData->lfuse, optibootData->hfuse, optibootData->efuse);
    progState++;
    break;

  case stateProg:
    // Not sure what to do here.
    break;
  }
}

int ICACHE_FLASH_ATTR cgiMegaRead(HttpdConnData *connData) {
  // DBG("cgiMegaRead %s\n", connData->url);

  int p, i, len;
  char *buffer, s[12];

  debug_cnt++;

  // sscanf(connData->url + 14, "0x%x,%x", &p, &len);
  char *ptr = connData->url + 14;
  p = strtoul(ptr, &ptr, 16);
  ptr++;	// skip comma
  len = strtoul(ptr, NULL, 16);
  // DBG("cgiMegaRead : %x %x\n", p, len);

  buffer = os_malloc(len / 16 * 60);
  if (buffer == 0) {
    espconn_send(connData->conn, (uint8_t *)"Cannot allocate sufficient memory\n", 35);
    return HTTPD_CGI_DONE;
  }
  buffer[0] = 0;

  // initPacket();
  sendLoadAddressQuery(p);
  readLoadAddressReply();
  if (! reply_ok) {
    espconn_send(connData->conn, (uint8_t *)"Load address failed\n", 20);
    return HTTPD_CGI_DONE;
  }

  for (i=0; i<len; i+=16) {
    char flash[20];
    sendReadFlashQuery();
    readReadFlashReply(flash);
    if (! reply_ok) {
      espconn_send(connData->conn, (uint8_t *)"Unknown problem\n", 16);
      return HTTPD_CGI_DONE;
    }

    int j;

    os_sprintf(s, "%08x: ", p + i);
    strcat(buffer, s);

    for (j=0; j<16; j++) {
      os_sprintf(s, "%02x ", flash[j]);
      strcat(buffer, s);
    }
    strcat(buffer, "\n");
  }
  espconn_send(connData->conn, (uint8_t *)buffer, strlen(buffer));

  // cleanupPacket();

  // espconn_sent(connData->conn, "This is the output ... :-)\n", 27);
  return HTTPD_CGI_DONE;
}

void ICACHE_FLASH_ATTR sendReadFuseQuery(char fuse) {
  pbuf.ms1 = 0;
  pbuf.ms2 = 8;
  pbuf.body[0] = CMD_SPI_MULTI;		// 0x1D
  pbuf.body[1] = 4;
  pbuf.body[2] = 4;
  pbuf.body[3] = 0;
  switch (fuse) {
  case 'l':
    pbuf.body[4] = 0x50; pbuf.body[5] = 0x00;
    break;
  case 'h':
    pbuf.body[4] = 0x58; pbuf.body[5] = 0x08;
    break;
  case 'e':
    pbuf.body[4] = 0x50; pbuf.body[5] = 0x08;
    break;
  }
  pbuf.body[6] = 0;
  pbuf.body[7] = 0;

  writePacket();
}

/*
 * To read the feedback directly.
 */
int ICACHE_FLASH_ATTR readReadFuseReply() {
  reply_ok = false;
  int len = readPacket();
  if (len != 7) {
    DBG("readReadFuseReply: packet len %d, expexted 13.\n", len);
    return -1;
  }
  if (pbuf.body[0] == CMD_SPI_MULTI && pbuf.body[1] == STATUS_CMD_OK) {
    reply_ok = true;
    return pbuf.body[5];
  }
  return 0;
}

/*
 * Called from megaUartRecv so read from the buffer passed as parameter,
 * don't call readPacket().
 */
int ICACHE_FLASH_ATTR getFuseReply(char *ptr, int len) {
  reply_ok = false;
  if (len != 13) {
    DBG("readReadFuseReply: packet len %d, expexted 13.\n", len);
    return -1;
  }
  if (ptr[4] != TOKEN && ptr[0] != MESSAGE_START)
    return -1;
  if (ptr[5] == CMD_SPI_MULTI && ptr[6] == STATUS_CMD_OK) {
    reply_ok = true;
    return ptr[10];
  }
  return 0;
}

int ICACHE_FLASH_ATTR readFuse(char fuse) {
  sendReadFuseQuery(fuse);
  int x = readReadFuseReply();
  return x;
}

/*
 * /pgmmega/fuse/r1 reads fuse 1
 * /pgmmega/fuse/1 writes fuse 1
 */
int ICACHE_FLASH_ATTR cgiMegaFuse(HttpdConnData *connData) {
  DBG("cgiMegaFuse %s\n", connData->url);

  // decode url
  char fuse = 'l';

  // read it
  int x = readFuse(fuse);

  // provide result

  // handle error cases
  if (reply_ok) {
    char buf[20];
    os_sprintf(buf, "Fuse '%c' : 0x%x", fuse, x);

    noCacheHeaders(connData, 200);
    httpdEndHeaders(connData);
    httpdSend(connData, buf, 0);
  } else {
    errorResponse(connData, 404, "Failed to reboot the MCU");
  }

  return HTTPD_CGI_DONE;
}

/*
 * Reboot the MCU, after which it will be in Arduino mode and we'll have lost
 * synchronized communication with it.
 */
int ICACHE_FLASH_ATTR cgiMegaRebootMCU(HttpdConnData *connData) {
  sendRebootMCUQuery();
  readRebootMCUReply();

  if (reply_ok) {
    noCacheHeaders(connData, 200);
    httpdEndHeaders(connData);
    httpdSend(connData, "", 0);
  } else {
    errorResponse(connData, 404, "Failed to reboot the MCU");
  }
  return HTTPD_CGI_DONE;
}

/*
 * Several versions of a function to selectively enable debug output
 */
static void ICACHE_FLASH_ATTR debug_reset() {
  debug_cnt = 0;
  debugOn(false);
}

static bool debug_on = false;

static bool ICACHE_FLASH_ATTR debug() {
#if 0
  debug_cnt++;
  if (debug_cnt < 30)
    return true;
  return false;
#endif
#if 0
  return true;
#endif

#if 0
  if (debug_cnt > 0)
    return true;
  return false;
#endif

#if 0
  if (optibootData == 0)
    return false;
  if ((0x0000F000 <= optibootData->address) && (optibootData->address <= 0x00010200))
    return true;
  return false;
#endif

  return debug_on;
}

static void debugOn(bool on) {
  debug_on = on;
}
