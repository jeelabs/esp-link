#include "EspLink.h"

#define READ_BUF_DFLT_SIZE 64

// Standard SLIP escape chars from RFC
#define SLIP_END      0300    // indicates end of packet
#define SLIP_ESC      0333    // indicates byte stuffing
#define SLIP_ESC_END  0334    // ESC ESC_END means END data byte
#define SLIP_ESC_ESC  0335    // ESC ESC_ESC means ESC data byte

EspLink::EspLink(Stream &streamIn, CmdRequestCB callback):stream(streamIn),requestCb(callback)
{
  readBuf = NULL;
  readLastChar = 0;
}

EspLink::~EspLink()
{
  if( readBuf != NULL )
    free( readBuf );
  readBuf = NULL;
}
    
void EspLink::writeChar(uint8_t data)
{
  switch(data)
  {
  case SLIP_END:
    stream.write(SLIP_ESC);
    stream.write(SLIP_ESC_END);
    break;
  case SLIP_ESC:
    stream.write(SLIP_ESC);
    stream.write(SLIP_ESC_ESC);
    break;
  default:
    stream.write(data);
  }

  crc16_add(data, &crc16_out);
}

/* CITT CRC16 polynomial ^16 + ^12 + ^5 + 1 */
/*---------------------------------------------------------------------------*/
void EspLink::crc16_add(uint8_t b, uint16_t *crc)
{
  *crc ^= b;
  *crc  = (*crc >> 8) | (*crc << 8);
  *crc ^= (*crc & 0xff00) << 4;
  *crc ^= (*crc >> 8) >> 4;
  *crc ^= (*crc & 0xff00) >> 5;
}

void EspLink::writeBuf(uint8_t * buf, uint16_t len)
{
  while(len-- > 0)
    writeChar(*buf++);
}

void EspLink::sendPacketStart(uint16_t cmd, uint32_t value, uint16_t argc)
{
  crc16_out = 0;
  stream.write( SLIP_END );
  writeBuf((uint8_t*)&cmd, 2);
  writeBuf((uint8_t*)&argc, 2);
  writeBuf((uint8_t*)&value, 4);
}

void EspLink::sendPacketArg(uint16_t len, uint8_t * data)
{
  writeBuf((uint8_t*)&len, 2);
  writeBuf(data, len);

  uint16_t pad = (4-((len+2)&3))&3; // get to multiple of 4
  if (pad > 0) {
    uint32_t temp = 0;
    writeBuf((uint8_t*)&temp, pad);
  }
}

void EspLink::sendPacketEnd() {
  uint16_t crc = crc16_out;
  writeBuf((uint8_t*)&crc, 2);
  stream.write(SLIP_END);
}

void EspLink::parseSlipPacket()
{
  CmdRequest req;
  req.cmd = (CmdPacket *)readBuf;
  req.arg_num = 0;
  req.arg_ptr = readBuf + sizeof(CmdPacket);

  requestCb(&req);

  free(readBuf);
  readBuf = NULL;
}

void EspLink::checkPacket()
{
  if( readBufPtr <= 3 )
    return;
  uint16_t crc = 0;
  for(uint16_t i=0; i < readBufPtr - 2; i++)
    crc16_add(readBuf[i], &crc);

  uint16_t crcpacket = *(uint16_t*)(readBuf + readBufPtr - 2);

  if( crc == crcpacket )
  {
    readBufPtr -= 2;
    parseSlipPacket();
  }
}

void EspLink::readLoop()
{
  if( stream.available() > 0 )
  {
    int byt = stream.read();

    switch(readState)
    {
      case WAIT_FOR_SLIP_START:
        if( byt == SLIP_END )
        {
          if(readBuf != NULL)
            free(readBuf);
          readBufPtr = 0;
          readBufMax = READ_BUF_DFLT_SIZE;
          readBuf = (uint8_t *)malloc(readBufMax);
          readState = READ_SLIP_PACKAGE;
        }
        break;
      case READ_SLIP_PACKAGE:
        if( byt == SLIP_END )
        {
          readState = WAIT_FOR_SLIP_START;
          checkPacket();
          break;
        }
        if( byt == SLIP_ESC )
          break;
        if( readLastChar == SLIP_ESC && byt == SLIP_ESC_END )
          byt = SLIP_END;
        else if( readLastChar == SLIP_ESC && byt == SLIP_ESC_ESC )
          byt = SLIP_ESC;

        if( readBufPtr >= readBufMax )
        {
          readBufMax = readBufMax + READ_BUF_DFLT_SIZE;
          readBuf = (uint8_t *)realloc(readBuf, readBufMax);
          if( readBuf == NULL )
          {
            readState = WAIT_FOR_SLIP_START; // TODO
            break;
          }
        }
        readBuf[readBufPtr++] = byt;
        break;
    }

    readLastChar = byt;
  }
}

// Return the number of arguments given a command struct
uint32_t EspLink::cmdGetArgc(CmdRequest *req) {
  return req->cmd->argc;
}

// Copy the next argument from a command structure into the data pointer, returns 0 on success
// -1 on error
int32_t EspLink::cmdPopArg(CmdRequest *req, void *data, uint16_t len) {
  uint16_t length;

  if (req->arg_num >= req->cmd->argc)
    return -1;

  length = *(uint16_t*)req->arg_ptr;
  if (length != len) return -1; // safety check

  memcpy(data, req->arg_ptr + 2, length);
  req->arg_ptr += (length+5)&~3; // round up to multiple of 4

  req->arg_num ++;
  return 0;
}

// Skip the next argument
void EspLink::cmdSkipArg(CmdRequest *req) {
  uint16_t length;

  if (req->arg_num >= req->cmd->argc) return;

  length = *(uint16_t*)req->arg_ptr;

  req->arg_ptr += (length+5)&~3;
  req->arg_num ++;
}

// Return the length of the next argument
uint16_t EspLink::cmdArgLen(CmdRequest *req) {
  return *(uint16_t*)req->arg_ptr;
}

