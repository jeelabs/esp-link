// Copyright (c) 2015 by Thorsten von Eicken, see LICENSE.txt in the esp-link repo

// Some code moved to esp-link/pgmshared.c to avoid code duplication.
// Those changes are Copyright (c) 2017 by Danny Backx.

// Protocol used : https://github.com/Optiboot/optiboot/wiki/HowOptibootWorks

#include <esp8266.h>
#include "cgi.h"
#include "cgioptiboot.h"
#include "config.h"
#include "uart.h"
#include "stk500.h"
#include "serbridge.h"
#include "mqtt_cmd.h"
#include "serled.h"

#include "pgmshared.h"

#define INIT_DELAY     150   // wait this many millisecs before sending anything
#define BAUD_INTERVAL  600   // interval after which we change baud rate
#define PGM_TIMEOUT  20000   // timeout after sync is achieved, in milliseconds
#define PGM_INTERVAL   200   // send sync at this interval in ms when in programming mode
#define ATTEMPTS         8   // number of attempts total to make

#ifdef OPTIBOOT_DBG
#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)
#else
#define DBG(format, ...) do { } while(0)
#endif

#define DBG_GPIO5 1 // define to 1 to use GPIO5 to trigger scope

//===== global state

static ETSTimer optibootTimer;

static enum {                // overall programming states
  stateInit = 0,             // initial delay
  stateSync,                 // waiting to hear back
  stateGetSig,               // reading device signature
  stateGetVersLo,            // reading optiboot version, low bits
  stateGetVersHi,            // reading optiboot version, high bits
  stateProg,                 // programming...
} progState;
static char* progStates[] = { "init", "sync", "sig", "ver0", "ver1", "prog" };
static short baudCnt;        // counter for sync attempts at different baud rates
static short ackWait;        // counter of expected ACKs
static uint16_t optibootVers;
static uint32_t baudRate;    // baud rate at which we're programming

#define MAX_PAGE_SZ 512      // max flash page size supported
#define MAX_SAVED   512      // max chars in saved buffer

// forward function references
static void optibootTimerCB(void *);
static void optibootUartRecv(char *buffer, short length);
static void armTimer(uint32_t ms);
static void initBaud(void);

static void ICACHE_FLASH_ATTR optibootInit() {
  progState = stateInit;
  baudCnt = 0;
  uart0_baud(flashConfig.baud_rate);
  mqtt_unblock();
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
}

// append one string to another but visually escape non-printing characters in the second
// string using \x00 hex notation, max is the max chars in the concatenated string.
void ICACHE_FLASH_ATTR appendPretty(char *buf, int max, char *raw, int rawLen) {
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
int ICACHE_FLASH_ATTR cgiOptibootSync(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

  // check that we know the reset pin, else error out with that
  if (flashConfig.reset_pin < 0) {
    errorResponse(connData, 400, "No reset pin defined");

  } else if (connData->requestType == HTTPD_METHOD_POST) {
    // issue reset
    optibootInit();
    mqtt_block(); // prevent MQTT from interfering
    baudRate = flashConfig.baud_rate;
    programmingCB = optibootUartRecv;
    initBaud();
    serbridgeReset();
#if DBG_GPIO5
    makeGpio(5);
    gpio_output_set(0, (1<<5), (1<<5), 0); // output 0
#endif

    // start sync timer
    os_timer_disarm(&optibootTimer);
    os_timer_setfn(&optibootTimer, optibootTimerCB, NULL);
    os_timer_arm(&optibootTimer, INIT_DELAY, 0);

    // respond with optimistic OK
    noCacheHeaders(connData, 204);
    httpdEndHeaders(connData);
    httpdSend(connData, "", 0);

  } else if (connData->requestType == HTTPD_METHOD_GET) {
    noCacheHeaders(connData, 200);
    httpdEndHeaders(connData);
    if (!errMessage[0] && progState >= stateProg) {
      char buf[64];
      DBG("OB got sync\n");
      os_sprintf(buf, "SYNC at %d baud: bootloader v%d.%d",
          baudRate, optibootVers>>8, optibootVers&0xff);
      httpdSend(connData, buf, -1);
    } else if (errMessage[0] && progState == stateSync) {
      DBG("OB cannot sync\n");
      char buf[512];
      os_sprintf(buf, "FAILED to SYNC: %s, got: %d chars\r\n", errMessage, responseLen);
      appendPretty(buf, 512, responseBuf, responseLen);
      httpdSend(connData, buf, -1);
    } else {
      httpdSend(connData, errMessage[0] ? errMessage : "NOT READY", -1);
    }

  } else {
    errorResponse(connData, 404, "Only GET and POST supported");
  }

  return HTTPD_CGI_DONE;
}

//===== Cgi to write firmware to Optiboot, requires prior sync call
int ICACHE_FLASH_ATTR cgiOptibootData(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  if (!optibootData)
    DBG("OB pgm: state=%d postLen=%d\n", progState, connData->post->len);

  // check that we have sync
  if (errMessage[0] || progState < stateProg) {
    DBG("OB not in sync, state=%d, err=%s\n", progState, errMessage);
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
    optibootData = os_zalloc(sizeof(struct optibootData));
    char *saved = os_zalloc(MAX_SAVED+1); // need space for string terminator
    char *pageBuf = os_zalloc(MAX_PAGE_SZ+MAX_SAVED/2);
    if (!optibootData || !pageBuf || !saved) {
      errorResponse(connData, 400, "Out of memory");
      return HTTPD_CGI_DONE;
    }
    optibootData->mega = false;
    optibootData->pageBuf = pageBuf;
    optibootData->saved = saved;
    optibootData->startTime = system_get_time();
    optibootData->pgmSz = 128; // hard coded for 328p for now, should be query string param
    DBG("OB data alloc\n");
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
    // tell optiboot to reboot into the sketch
    uart0_write_char(STK_LEAVE_PROGMODE);
    uart0_write_char(CRC_EOP);
    code = 200;
    // calculate some stats
    float dt = ((system_get_time() - optibootData->startTime)/1000)/1000.0; // in seconds
    uint16_t pgmDone = optibootData->pgmDone;
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

// Poll UART for ACKs, max 50ms
static bool pollAck() {
  char recv[16];
  uint16_t need = ackWait*2;
  uint16_t got = uart0_rx_poll(recv, need, 50000);
#ifdef DBG_GPIO5
  gpio_output_set(0, (1<<5), (1<<5), 0); // output 0
#endif
  if (got < need) {
    os_strcpy(errMessage, "Timeout waiting for flash page to be programmed");
    return false;
  }
  ackWait = 0;
  if (recv[0] == STK_INSYNC && recv[1] == STK_OK)
    return true;
  os_sprintf(errMessage, "Did not get ACK after programming cmd: %x02x %x02x", recv[0], recv[1]);
  return false;
}

// Program a flash page
bool ICACHE_FLASH_ATTR optibootProgramPage(void) {
  if (optibootData->pageLen == 0) return true;
  armTimer(PGM_TIMEOUT); // keep the timerCB out of the picture

  if (ackWait > 7) {
    os_sprintf(errMessage, "Lost sync while programming\n");
    return false;
  }

  uint16_t pgmLen = optibootData->pageLen;
  if (pgmLen > optibootData->pgmSz) pgmLen = optibootData->pgmSz;
  DBG("OB pgm %d@0x%x\n", pgmLen, optibootData->address);

  // send address to optiboot (little endian format)
#ifdef DBG_GPIO5
  gpio_output_set((1<<5), 0, (1<<5), 0); // output 1
#endif
  ackWait++;
  uart0_write_char(STK_LOAD_ADDRESS);
  uint16_t addr = optibootData->address >> 1; // word address
  uart0_write_char(addr & 0xff);
  uart0_write_char(addr >> 8);
  uart0_write_char(CRC_EOP);
  armTimer(PGM_TIMEOUT);
  if (!pollAck()) {
    DBG("OB pgm failed in load address\n");
    return false;
  }
  armTimer(PGM_TIMEOUT);

  // send page length (big-endian format, go figure...)
#ifdef DBG_GPIO5
  gpio_output_set((1<<5), 0, (1<<5), 0); // output 1
#endif
  ackWait++;
  uart0_write_char(STK_PROG_PAGE);
  uart0_write_char(pgmLen>>8);
  uart0_write_char(pgmLen&0xff);
  uart0_write_char('F'); // we're writing flash

  // send page content
  for (short i=0; i<pgmLen; i++)
    uart0_write_char(optibootData->pageBuf[i]);
  uart0_write_char(CRC_EOP);

  armTimer(PGM_TIMEOUT);
  bool ok = pollAck();
  armTimer(PGM_TIMEOUT);
  if (!ok) {
    DBG("OB pgm failed in prog page\n");
    return false;
  }

  // shift data out of buffer
  os_memmove(optibootData->pageBuf, optibootData->pageBuf+pgmLen, optibootData->pageLen-pgmLen);
  optibootData->pageLen -= pgmLen;
  optibootData->address += pgmLen;
  optibootData->pgmDone += pgmLen;

  //DBG("OB pgm OK\n");
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

static void ICACHE_FLASH_ATTR optibootTimerCB(void *arg) {
  // see whether we've issued so many sync in a row that it's time to give up
  switch (progState) {
    case stateInit: // initial delay expired, send sync chars
        uart0_write_char(STK_GET_SYNC);
        uart0_write_char(CRC_EOP);
        progState++;
        armTimer(BAUD_INTERVAL-INIT_DELAY);
        return;
    case stateSync: // oops, must have not heard back!?
      if (baudCnt > ATTEMPTS) {
        // we're doomed, give up
        DBG("OB abandoned after %d attempts\n", baudCnt);
        optibootInit();
        strcpy(errMessage, "sync abandoned after 8 attempts");
        return;
      }
      // time to switch baud rate and issue a reset
      DBG("OB no sync response @%d baud\n", baudRate);
      setBaud();
      serbridgeReset();
      progState = stateInit;
      armTimer(INIT_DELAY);
      return;
    case stateProg: // we're programming and we timed-out of inaction
      uart0_write_char(STK_GET_SYNC);
      uart0_write_char(CRC_EOP);
      ackWait++; // we now expect an ACK
      armTimer(PGM_INTERVAL);
      return;
    default: // we're trying to get some info from optiboot and it should have responded!
      optibootInit(); // abort
      os_sprintf(errMessage, "No response in state %s(%d) @%d baud\n",
          progStates[progState], progState, baudRate);
      DBG("OB %s\n", errMessage);
      return; // do not re-arm timer
  }
}

#if 0
static void ICACHE_FLASH_ATTR print_buff(char *msg, char *buf, short length) {
  DBG("OB GOT %s %d:", msg, length);
  for (int i=0; i<length; i++) DBG(" %x", buf[i]);
  DBG("\n");
}
#endif

// skip in-sync responses
static short ICACHE_FLASH_ATTR skipInSync(char *buf, short length) {
  while (length > 1 && buf[0] == STK_INSYNC && buf[1] == STK_OK) {
    // not the most efficient, oh well...
    os_memcpy(buf, buf+2, length-2);
    length -= 2;
  }
  return length;
}

// receive response from optiboot, we only store the last response
static void ICACHE_FLASH_ATTR optibootUartRecv(char *buf, short length) {
  //print_buff("RAW", buf, length);
  // append what we got to what we have accumulated
  if (responseLen < RESP_SZ-1) {
    char *rb = responseBuf+responseLen;
    for (short i=0; i<length && (rb-responseBuf)<(RESP_SZ-1); i++)
      if (buf[i] != 0) *rb++ = buf[i]; // don't copy NULL characters, TODO: fix it
    responseLen = rb-responseBuf;
    responseBuf[responseLen] = 0; // string terminator
  }
  //print_buff("RBUF", responseBuf, responseLen);

  // dispatch based the current state
  switch (progState) {
  case stateInit:  // we haven't sent anything, this must be garbage
    break;
  case stateSync: // we're trying to get a sync response
    // look for STK_INSYNC+STK_OK at end of buffer
    if (responseLen > 0 && responseBuf[responseLen-1] == STK_INSYNC) {
      // missing STK_OK after STK_INSYNC, shift stuff out and try again
      responseBuf[0] = STK_INSYNC;
      responseLen = 1;
    } else if (responseLen > 1 && responseBuf[responseLen-2] == STK_INSYNC &&
        responseBuf[responseLen-1] == STK_OK) {
      // got sync response
      responseLen = 0; // ignore anything that may have accumulated
      // send request to get signature
      uart0_write_char(STK_READ_SIGN);
      uart0_write_char(CRC_EOP);
      progState++;
      armTimer(PGM_INTERVAL); // reset timer
      //DBG("OB: got sync, sent read-sig\n");
    } else {
      // nothing useful, keep at most half the buffer for error message purposes
      if (responseLen > RESP_SZ/2) {
        os_memcpy(responseBuf, responseBuf+responseLen-RESP_SZ/2, RESP_SZ/2);
        responseLen = RESP_SZ/2;
        responseBuf[responseLen] = 0; // string terminator
      }
    }
    break;
  case stateGetSig: // expecting signature
    responseLen = skipInSync(responseBuf, responseLen);
    if (responseLen >= 5 && responseBuf[0] == STK_INSYNC && responseBuf[4] == STK_OK) {
      if (responseBuf[1] == 0x1e && responseBuf[2] == 0x95 && responseBuf[3] == 0x0f) {
        // right on... ask for optiboot version
        progState++;
        uart0_write_char(STK_GET_PARAMETER);
        uart0_write_char(0x82);
        uart0_write_char(CRC_EOP);
        armTimer(PGM_INTERVAL); // reset timer
      } else {
        optibootInit(); // abort
        os_sprintf(errMessage, "Bad programmer signature: 0x%02x 0x%02x 0x%02x\n",
            responseBuf[1], responseBuf[2], responseBuf[3]);
      }
      os_memcpy(responseBuf, responseBuf+5, responseLen-5);
      responseLen -= 5;
    }
    break;
  case stateGetVersLo: // expecting version
    if (responseLen >= 3 && responseBuf[0] == STK_INSYNC && responseBuf[2] == STK_OK) {
      optibootVers = responseBuf[1];
      progState++;
      os_memcpy(responseBuf, responseBuf+3, responseLen-3);
      responseLen -= 3;
      uart0_write_char(STK_GET_PARAMETER);
      uart0_write_char(0x81);
      uart0_write_char(CRC_EOP);
      armTimer(PGM_INTERVAL); // reset timer
    }
    break;
  case stateGetVersHi: // expecting version
    if (responseLen >= 3 && responseBuf[0] == STK_INSYNC && responseBuf[2] == STK_OK) {
      optibootVers |= responseBuf[1]<<8;
      progState++;
      os_memcpy(responseBuf, responseBuf+3, responseLen-3);
      responseLen -= 3;
      armTimer(PGM_INTERVAL); // reset timer
      ackWait = 0;
    }
    break;
  case stateProg: // count down expected sync responses
    //DBG("UART recv %d\n", length);
    while (responseLen >= 2 && responseBuf[0] == STK_INSYNC && responseBuf[1] == STK_OK) {
      if (ackWait > 0) ackWait--;
      os_memmove(responseBuf, responseBuf+2, responseLen-2);
      responseLen -= 2;
    }
    armTimer(PGM_INTERVAL); // reset timer
    break;
  default:
    break;
  }
}

