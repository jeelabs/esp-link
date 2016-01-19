// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt
//
// Adapted from: github.com/tuanpmt/esp_bridge, Created on: Jan 9, 2015, Author: Minh

#include "esp8266.h"
#include "cmd.h"
#include "crc16.h"
#include "uart.h"

#ifdef CMD_DBG
#define DBG(format, ...) os_printf(format, ## __VA_ARGS__)
static const char *cmd_names[] = {
  "NULL", "RESET", "IS_READY", "WIFI_CONNECT",
  "MQTT_SETUP", "MQTT_CONNECT", "MQTT_DISCONNECT",
  "MQTT_PUBLISH", "MQTT_SUBSCRIBE", "MQTT_LWT", "MQTT_EVENTS",
  "REST_SETUP", "REST_REQUEST", "REST_SETHEADER", "REST_EVENTS",
  "CB_ADD", "CB_EVENTS",
};
#else
#define DBG(format, ...) do { } while(0)
#endif

extern const CmdList commands[];

//===== ESP -> Serial responses

static void ICACHE_FLASH_ATTR
CMD_ProtoWrite(uint8_t data) {
  switch(data){
  case SLIP_START:
  case SLIP_END:
  case SLIP_REPL:
    uart0_write_char(SLIP_REPL);
    uart0_write_char(SLIP_ESC(data));
    break;
  default:
    uart0_write_char(data);
  }
}

static void ICACHE_FLASH_ATTR
CMD_ProtoWriteBuf(uint8_t *data, short len) {
  while (len--) CMD_ProtoWrite(*data++);
}

// Start a response, returns the partial CRC
uint16_t ICACHE_FLASH_ATTR
CMD_ResponseStart(uint16_t cmd, uint32_t callback, uint32_t _return, uint16_t argc) {
  uint16_t crc = 0;

  uart0_write_char(SLIP_START);
  CMD_ProtoWriteBuf((uint8_t*)&cmd, 2);
  crc = crc16_data((uint8_t*)&cmd, 2, crc);
  CMD_ProtoWriteBuf((uint8_t*)&callback, 4);
  crc = crc16_data((uint8_t*)&callback, 4, crc);
  CMD_ProtoWriteBuf((uint8_t*)&_return, 4);
  crc = crc16_data((uint8_t*)&_return, 4, crc);
  CMD_ProtoWriteBuf((uint8_t*)&argc, 2);
  crc = crc16_data((uint8_t*)&argc, 2, crc);
  return crc;
}

// Adds data to a response, returns the partial CRC
uint16_t ICACHE_FLASH_ATTR
CMD_ResponseBody(uint16_t crc_in, uint8_t* data, short len) {
  short pad_len = len+3 - (len+3)%4; // round up to multiple of 4
  CMD_ProtoWriteBuf((uint8_t*)&pad_len, 2);
  crc_in = crc16_data((uint8_t*)&pad_len, 2, crc_in);

  CMD_ProtoWriteBuf(data, len);
  crc_in = crc16_data(data, len, crc_in);

  if (pad_len > len) {
    uint32_t temp = 0;
    CMD_ProtoWriteBuf((uint8_t*)&temp, pad_len-len);
    crc_in = crc16_data((uint8_t*)&temp, pad_len-len, crc_in);
  }

  return crc_in;
}

// Ends a response
void ICACHE_FLASH_ATTR
CMD_ResponseEnd(uint16_t crc) {
  CMD_ProtoWriteBuf((uint8_t*)&crc, 2);
  uart0_write_char(SLIP_END);
}

//===== serial -> ESP commands

// Execute a parsed command
static uint32_t ICACHE_FLASH_ATTR
CMD_Exec(const CmdList *scp, CmdPacket *packet) {
  uint16_t crc = 0;
  // Iterate through the command table and call the appropriate function
  while (scp->sc_function != NULL) {
    if(scp->sc_name == packet->cmd) {
      DBG("CMD_Exec: Dispatching cmd=%s\n", cmd_names[packet->cmd]);
      // call command function
      uint32_t ret = scp->sc_function(packet);
      // if requestor asked for a response, send it
      if (packet->_return){
        DBG("CMD_Exec: Response: 0x%lx, cmd: %d\r\n", ret, packet->cmd);
        crc = CMD_ResponseStart(packet->cmd, 0, ret, 0);
        CMD_ResponseEnd(crc);
      } else {
        DBG("CMD_Exec: no response (%lu)\n", packet->_return);
      }
      return ret;
    }
    scp++;
  }
  DBG("CMD_Exec: cmd=%d not found\n", packet->cmd);
  return 0;
}

// Parse a packet and print info about it
void ICACHE_FLASH_ATTR
CMD_parse_packet(uint8_t *buf, short len) {
  // minimum command length
  if (len < 12) return;

  // init pointers into buffer
  CmdPacket *packet = (CmdPacket*)buf;
  uint8_t *data_ptr = (uint8_t*)&packet->args;
  uint8_t *data_limit = data_ptr+len;

  DBG("CMD_parse_packet: cmd=%d(%s) argc=%d cb=%p ret=%lu\n",
      packet->cmd,
      cmd_names[packet->cmd],
      packet->argc,
      (void *)packet->callback,
      packet->_return
  );

#if 0
  // print out arguments
  uint16_t argn = 0;
  uint16_t argc = packet->argc;
  while (data_ptr+2 < data_limit && argc--) {
    short l = *(uint16_t*)data_ptr;
    os_printf("CMD_parse_packet: arg[%d] len=%d:", argn++, l);
    data_ptr += 2;
    while (data_ptr < data_limit && l--) {
      os_printf(" %02X", *data_ptr++);
    }
    os_printf("\n");
  }
#endif

  if (data_ptr <= data_limit) {
    CMD_Exec(commands, packet);
  } else {
    DBG("CMD_parse_packet: packet length overrun, parsing arg %d\n", packet->argc);
  }
}

//===== Helpers to parse a command packet

// Fill out a CmdRequest struct given a CmdPacket
void ICACHE_FLASH_ATTR
CMD_Request(CmdRequest *req, CmdPacket* cmd) {
  req->cmd = cmd;
  req->arg_num = 0;
  req->arg_ptr = (uint8_t*)&cmd->args;
}

// Return the number of arguments given a command struct
uint32_t ICACHE_FLASH_ATTR
CMD_GetArgc(CmdRequest *req) {
  return req->cmd->argc;
}

// Copy the next argument from a command structure into the data pointer, returns 0 on success
// -1 on error
int32_t ICACHE_FLASH_ATTR
CMD_PopArg(CmdRequest *req, void *data, uint16_t len) {
  uint16_t length;

  if (req->arg_num >= req->cmd->argc)
    return -1;

  length = *(uint16_t*)req->arg_ptr;
  if (length != len) return -1; // safety check

  req->arg_ptr += 2;
  os_memcpy(data, req->arg_ptr, length);
  req->arg_ptr += length;

  req->arg_num ++;
  return 0;
}

// Skip the next argument
void ICACHE_FLASH_ATTR
CMD_SkipArg(CmdRequest *req) {
  uint16_t length;

  if (req->arg_num >= req->cmd->argc) return;

  length = *(uint16_t*)req->arg_ptr;

  req->arg_ptr += 2;
  req->arg_ptr += length;
  req->arg_num ++;
}

// Return the length of the next argument
uint16_t ICACHE_FLASH_ATTR
CMD_ArgLen(CmdRequest *req) {
  return *(uint16_t*)req->arg_ptr;
}
