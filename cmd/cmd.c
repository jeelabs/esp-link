// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt
//
// Adapted from: github.com/tuanpmt/esp_bridge, Created on: Jan 9, 2015, Author: Minh

#include "esp8266.h"
#include "cmd.h"
#include "crc16.h"
#include "uart.h"

#ifdef CMD_DBG
#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)
#else
#define DBG(format, ...) do { } while(0)
#endif

//===== ESP -> Serial responses

static void ICACHE_FLASH_ATTR
cmdProtoWrite(uint8_t data) {
  switch(data){
  case SLIP_END:
    uart0_write_char(SLIP_ESC);
    uart0_write_char(SLIP_ESC_END);
    break;
  case SLIP_ESC:
    uart0_write_char(SLIP_ESC);
    uart0_write_char(SLIP_ESC_ESC);
    break;
  default:
    uart0_write_char(data);
  }
}

static void ICACHE_FLASH_ATTR
cmdProtoWriteBuf(const uint8_t *data, short len) {
  while (len--) cmdProtoWrite(*data++);
}

static uint16_t resp_crc;

// Start a response, returns the partial CRC
void ICACHE_FLASH_ATTR
cmdResponseStart(uint16_t cmd, uint32_t value, uint16_t argc) {
  DBG("cmdResponse: cmd=%d val=%d argc=%d\n", cmd, value, argc);

  uart0_write_char(SLIP_END);
  cmdProtoWriteBuf((uint8_t*)&cmd, 2);
  resp_crc = crc16_data((uint8_t*)&cmd, 2, 0);
  cmdProtoWriteBuf((uint8_t*)&argc, 2);
  resp_crc = crc16_data((uint8_t*)&argc, 2, resp_crc);
  cmdProtoWriteBuf((uint8_t*)&value, 4);
  resp_crc = crc16_data((uint8_t*)&value, 4, resp_crc);
}

// Adds data to a response, returns the partial CRC
void ICACHE_FLASH_ATTR
cmdResponseBody(const void *data, uint16_t len) {
  cmdProtoWriteBuf((uint8_t*)&len, 2);
  resp_crc = crc16_data((uint8_t*)&len, 2, resp_crc);

  cmdProtoWriteBuf(data, len);
  resp_crc = crc16_data(data, len, resp_crc);

  uint16_t pad = (4-((len+2)&3))&3; // get to multiple of 4
  if (pad > 0) {
    uint32_t temp = 0;
    cmdProtoWriteBuf((uint8_t*)&temp, pad);
    resp_crc = crc16_data((uint8_t*)&temp, pad, resp_crc);
  }
}

// Ends a response
void ICACHE_FLASH_ATTR
cmdResponseEnd() {
  cmdProtoWriteBuf((uint8_t*)&resp_crc, 2);
  uart0_write_char(SLIP_END);
}

//===== serial -> ESP commands

// Execute a parsed command
static void ICACHE_FLASH_ATTR
cmdExec(const CmdList *scp, CmdPacket *packet) {
  // Iterate through the command table and call the appropriate function
  while (scp->sc_function != NULL) {
    if(scp->sc_name == packet->cmd) {
      DBG("cmdExec: Dispatching cmd=%s\n", scp->sc_text);
      // call command function
      scp->sc_function(packet);
      return;
    }
    scp++;
  }
  DBG("cmdExec: cmd=%d not found\n", packet->cmd);
}

// Parse a packet and print info about it
void ICACHE_FLASH_ATTR
cmdParsePacket(uint8_t *buf, short len) {
  // minimum command length
  if (len < sizeof(CmdPacket)) return;

  // init pointers into buffer
  CmdPacket *packet = (CmdPacket*)buf;
  uint8_t *data_ptr = (uint8_t*)&packet->args;
  uint8_t *data_limit = data_ptr+len;

  DBG("cmdParsePacket: cmd=%d argc=%d value=%u\n",
      packet->cmd,
      packet->argc,
      packet->value
  );

#if 0
  // print out arguments
  uint16_t argn = 0;
  uint16_t argc = packet->argc;
  while (data_ptr+2 < data_limit && argc--) {
    short l = *(uint16_t*)data_ptr;
    os_printf("cmdParsePacket: arg[%d] len=%d:", argn++, l);
    data_ptr += 2;
    while (data_ptr < data_limit && l--) {
      os_printf(" %02X", *data_ptr++);
    }
    os_printf("\n");
  }
#endif

  if (!cmdInSync && packet->cmd != CMD_SYNC) {
    // we have not received a sync, perhaps we reset? Tell MCU to do a sync
    cmdResponseStart(CMD_SYNC, 0, 0);
    cmdResponseEnd();
  } else if (data_ptr <= data_limit) {
    cmdExec(commands, packet);
  } else {
    DBG("cmdParsePacket: packet length overrun, parsing arg %d\n", packet->argc);
  }
}

//===== Helpers to parse a command packet

// Fill out a CmdRequest struct given a CmdPacket
void ICACHE_FLASH_ATTR
cmdRequest(CmdRequest *req, CmdPacket* cmd) {
  req->cmd = cmd;
  req->arg_num = 0;
  req->arg_ptr = (uint8_t*)&cmd->args;
}

// Return the number of arguments given a command struct
uint32_t ICACHE_FLASH_ATTR
cmdGetArgc(CmdRequest *req) {
  return req->cmd->argc;
}

// Copy the next argument from a command structure into the data pointer, returns 0 on success
// -1 on error
int32_t ICACHE_FLASH_ATTR
cmdPopArg(CmdRequest *req, void *data, uint16_t len) {
  uint16_t length;

  if (req->arg_num >= req->cmd->argc)
    return -1;

  length = *(uint16_t*)req->arg_ptr;
  if (length != len) return -1; // safety check

  req->arg_ptr += 2;
  os_memcpy(data, req->arg_ptr, length);
  req->arg_ptr += (length+3)&~3; // round up to multiple of 4

  req->arg_num ++;
  return 0;
}

// Skip the next argument
void ICACHE_FLASH_ATTR
cmdSkipArg(CmdRequest *req) {
  uint16_t length;

  if (req->arg_num >= req->cmd->argc) return;

  length = *(uint16_t*)req->arg_ptr;

  req->arg_ptr += 2;
  req->arg_ptr += (length+3)&~3;
  req->arg_num ++;
}

// Return the length of the next argument
uint16_t ICACHE_FLASH_ATTR
cmdArgLen(CmdRequest *req) {
  return *(uint16_t*)req->arg_ptr;
}
