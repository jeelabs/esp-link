#ifndef ESP_LINK_H
#define ESP_LINK_H

#include <inttypes.h>
#include <Stream.h>

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

typedef void (* CmdRequestCB)(CmdRequest *);

typedef enum {
  CMD_NULL = 0,
  CMD_SYNC,           // synchronize and clear
  CMD_RESP_V,         // response with a value
  CMD_RESP_CB,        // response with a callback
  CMD_WIFI_STATUS,    // get the current wifi status
  CMD_CB_ADD,
  CMD_CB_EVENTS,
  CMD_GET_TIME,       // get current time in seconds since the unix epoch

  CMD_MQTT_SETUP = 10,  // set-up callbacks
  CMD_MQTT_PUBLISH,     // publish a message
  CMD_MQTT_SUBSCRIBE,   // subscribe to a topic
  CMD_MQTT_LWT,         // set the last-will-topic and messge

  CMD_REST_SETUP = 20,
  CMD_REST_REQUEST,
  CMD_REST_SETHEADER,
  
  CMD_WEB_DATA = 30,
  CMD_WEB_REQ_CB,
} CmdName;

typedef enum
{
  WAIT_FOR_SLIP_START,
  READ_SLIP_PACKAGE,
} ReadState;

class EspLink
{
  private:
    uint16_t     crc16_out;
    Stream      &stream;
    ReadState    readState;
    uint8_t  *   readBuf;
    uint16_t     readBufPtr;
    uint16_t     readBufMax;
    int          readLastChar;
    CmdRequestCB requestCb;

    void crc16_add(uint8_t b, uint16_t *crc);
    void writeChar(uint8_t chr);
    void writeBuf(uint8_t * buf, uint16_t len);
    void checkPacket();
    void parseSlipPacket();

  public:
    EspLink(Stream &stream, CmdRequestCB callback);
    ~EspLink();

    void sendPacketStart(uint16_t cmd, uint32_t value, uint16_t argc);
    void sendPacketArg(uint16_t len, uint8_t * data);
    void sendPacketEnd();

    void readLoop();

    uint32_t cmdGetArgc(CmdRequest *req);
    int32_t  cmdPopArg(CmdRequest *req, void *data, uint16_t len);
    void     cmdSkipArg(CmdRequest *req);
    uint16_t cmdArgLen(CmdRequest *req);
};

#endif /* ESP_LINK_H */

