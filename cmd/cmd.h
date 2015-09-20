// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt
//
// Adapted from: github.com/tuanpmt/esp_bridge, Created on: Jan 9, 2015, Author: Minh

#ifndef CMD_H
#define CMD_H
#include <esp8266.h>

// Escape chars used by tuanpmt, dunno why he didn't use std ones...
#define SLIP_START  0x7E
#define SLIP_END    0x7F
#define SLIP_REPL   0x7D
#define SLIP_ESC(x) (x ^ 0x20)

#if 0
// Proper SLIP escape chars from RFC
#define SLIP_END      0300    // indicates end of packet
#define SLIP_ESC      0333    // indicates byte stuffing
#define SLIP_ESC_END  0334    // ESC ESC_END means END data byte
#define SLIP_ESC_ESC  0335    // ESC ESC_ESC means ESC data byte
#endif

typedef struct __attribute__((__packed__)) {
  uint16_t  len;      // length of data
  uint8_t   data[0];  // really data[len]
} CmdArg;

typedef struct __attribute__((__packed__)) {
  uint16_t  cmd;      // command to perform, from CmdName enum
  uint32_t  callback; // callback pointer to embed in response
  uint32_t  _return;  // return value to embed in response (?)
  uint16_t  argc;     // number of arguments to command
  CmdArg    args[0];  // really args[argc]
} CmdPacket;

typedef struct {
  CmdPacket *cmd;     // command packet header
  uint32_t  arg_num;  // number of args parsed
  uint8_t   *arg_ptr; // pointer to ??
} CmdRequest;

typedef enum {
  CMD_NULL = 0,
  CMD_RESET,          // reset esp (not honored in this implementation)
  CMD_IS_READY,       // health-check
  CMD_WIFI_CONNECT,   // (3) connect to AP (not honored in this implementation)
  CMD_MQTT_SETUP,
  CMD_MQTT_CONNECT,
  CMD_MQTT_DISCONNECT,
  CMD_MQTT_PUBLISH,
  CMD_MQTT_SUBSCRIBE,
  CMD_MQTT_LWT,
  CMD_MQTT_EVENTS,
  CMD_REST_SETUP,     // (11)
  CMD_REST_REQUEST,
  CMD_REST_SETHEADER,
  CMD_REST_EVENTS,
  CMD_CB_ADD,         // 15
  CMD_CB_EVENTS
} CmdName;

typedef uint32_t (*cmdfunc_t)(CmdPacket *cmd);

typedef struct {
  CmdName   sc_name;
  cmdfunc_t sc_function;
} CmdList;

#define CMD_CBNLEN 16
typedef struct {
  char name[CMD_CBNLEN];
  uint32_t callback;
} cmdCallback;

// Used by slip protocol to cause parsing of a received packet
void CMD_parse_packet(uint8_t *buf, short len);

// Return the info about a callback to the attached uC by name, these are callbacks that the
// attached uC registers using the ADD_SENSOR command
cmdCallback* CMD_GetCbByName(char* name);

// Responses

// Start a response, returns the partial CRC
uint16_t CMD_ResponseStart(uint16_t cmd, uint32_t callback, uint32_t _return, uint16_t argc);
// Adds data to a response, returns the partial CRC
uint16_t CMD_ResponseBody(uint16_t crc_in, uint8_t* data, short len);
// Ends a response
void CMD_ResponseEnd(uint16_t crc);

//void CMD_Response(uint16_t cmd, uint32_t callback, uint32_t _return, uint16_t argc, CmdArg* args[]);

// Requests

// Fill out a CmdRequest struct given a CmdPacket
void CMD_Request(CmdRequest *req, CmdPacket* cmd);
// Return the number of arguments given a request
uint32_t CMD_GetArgc(CmdRequest *req);
// Return the length of the next argument
uint16_t CMD_ArgLen(CmdRequest *req);
// Copy next arg from request into the data pointer, returns 0 on success, -1 on error
int32_t CMD_PopArg(CmdRequest *req, void *data, uint16_t len);
// Skip next arg
void CMD_SkipArg(CmdRequest *req);

#endif
