// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt
//
// Adapted from: github.com/tuanpmt/esp_bridge, Created on: Jan 9, 2015, Author: Minh

#ifndef CMD_H
#define CMD_H
#include <esp8266.h>

// keep track of whether we received a sync command from uC
extern bool cmdInSync;

// Standard SLIP escape chars from RFC
#define SLIP_END      0300    // indicates end of packet
#define SLIP_ESC      0333    // indicates byte stuffing
#define SLIP_ESC_END  0334    // ESC ESC_END means END data byte
#define SLIP_ESC_ESC  0335    // ESC ESC_ESC means ESC data byte

typedef struct __attribute__((__packed__)) {
  uint16_t  len;      // length of data
  uint8_t   data[0];  // really data[len]
} CmdArg;

typedef struct __attribute__((__packed__)) {
  uint16_t  cmd;      // command to perform, from CmdName enum
  uint16_t  argc;     // number of arguments to command
  uint32_t  value;    // callback pointer for response or first argument
  CmdArg    args[0];  // really args[argc]
} CmdPacket;

typedef struct {
  CmdPacket *cmd;     // command packet header
  uint32_t  arg_num;  // number of args parsed
  uint8_t   *arg_ptr; // pointer to ??
} CmdRequest;

typedef enum {
  CMD_NULL = 0,
  CMD_SYNC,           // synchronize and clear
  CMD_RESP_V,         // response with a value
  CMD_RESP_CB,        // response with a callback
  CMD_WIFI_STATUS,    // get the current wifi status
  CMD_CB_ADD,
  CMD_CB_EVENTS,
  CMD_GET_TIME,       // get current time in seconds since the unix epoch
  CMD_GET_WIFI_INFO,	// query ip address info
  CMD_SET_WIFI_INFO,	// set ip address info

  CMD_MQTT_SETUP = 10,  // set-up callbacks
  CMD_MQTT_PUBLISH,     // publish a message
  CMD_MQTT_SUBSCRIBE,   // subscribe to a topic
  CMD_MQTT_LWT,         // set the last-will-topic and messge
  CMD_MQTT_GET_CLIENTID,

  CMD_REST_SETUP = 20,  // set-up callbacks
  CMD_REST_REQUEST,     // do REST request
  CMD_REST_SETHEADER,	// define header

  CMD_WEB_SETUP = 30,   // set-up WEB callback
  CMD_WEB_DATA,         // WEB data from MCU

  CMD_SOCKET_SETUP = 40, // set-up callbacks
  CMD_SOCKET_SEND,       // send data over UDP socket

  CMD_WIFI_GET_APCOUNT = 50,  // Query the number of networks / Access Points known
  CMD_WIFI_GET_APNAME,        // Query the name (SSID) of an Access Point (AP)
  CMD_WIFI_SELECT_SSID,       // Connect to a specific network
  CMD_WIFI_SIGNAL_STRENGTH,   // Query RSSI
  CMD_WIFI_GET_SSID,          // Query SSID currently connected to
  CMD_WIFI_START_SCAN,        // Trigger a scan (takes a long time)

} CmdName;

typedef void (*cmdfunc_t)(CmdPacket *cmd);

typedef struct {
  CmdName   sc_name;     // name as CmdName enum
  char      *sc_text;    // name as string
  cmdfunc_t sc_function; // pointer to function
} CmdList;

// command dispatch table
extern const CmdList commands[];

#define CMD_CBNLEN 16
typedef struct {
  char name[CMD_CBNLEN];
  uint32_t callback;
} CmdCallback;

// Used by slip protocol to cause parsing of a received packet
void cmdParsePacket(uint8_t *buf, short len);

// Return the info about a callback to the attached uC by name, these are callbacks that the
// attached uC registers using the ADD_SENSOR command
CmdCallback* cmdGetCbByName(char* name);

// Add a callback
uint32_t cmdAddCb(char *name, uint32_t callback);

// Responses

// Start a response
void cmdResponseStart(uint16_t cmd, uint32_t value, uint16_t argc);
// Adds data to a response
void cmdResponseBody(const void* data, uint16_t len);
// Ends a response
void cmdResponseEnd();

//void cmdResponse(uint16_t cmd, uint32_t callback, uint32_t value, uint16_t argc, CmdArg* args[]);

// Requests

// Fill out a CmdRequest struct given a CmdPacket
void cmdRequest(CmdRequest *req, CmdPacket* cmd);
// Return the number of arguments given a request
uint32_t cmdGetArgc(CmdRequest *req);
// Return the length of the next argument
uint16_t cmdArgLen(CmdRequest *req);
// Copy next arg from request into the data pointer, returns 0 on success, -1 on error
int32_t cmdPopArg(CmdRequest *req, void *data, uint16_t len);
// Skip next arg
void cmdSkipArg(CmdRequest *req);

#endif
