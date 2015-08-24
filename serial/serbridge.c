// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt

#include "espmissingincludes.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"
#include "gpio.h"

#include "uart.h"
#include "crc16.h"
#include "serbridge.h"
#include "serled.h"
#include "config.h"
#include "console.h"
#include "cmd.h"

static struct espconn serbridgeConn;
static esp_tcp serbridgeTcp;
static int8_t mcu_reset_pin, mcu_isp_pin;

extern uint8_t slip_disabled;   // disable slip to allow flashing of attached MCU

static sint8 ICACHE_FLASH_ATTR espbuffsend(serbridgeConnData *conn, const char *data, uint16 len);

// Connection pool
serbridgeConnData connData[MAX_CONN];
// Given a pointer to an espconn struct find the connection that correcponds to it
static serbridgeConnData ICACHE_FLASH_ATTR *serbridgeFindConnData(void *arg) {
  struct espconn *conn = arg;
  return arg == NULL ? NULL : (serbridgeConnData *)conn->reverse;
}

//===== TCP -> UART

// Telnet protocol characters
#define IAC        255  // escape
#define WILL       251  // negotiation
#define SB         250  // subnegotiation begin
#define SE         240  // subnegotiation end
#define ComPortOpt  44  // COM port options
#define SetControl   5  // Set control lines
#define DTR_ON       8  // used here to reset microcontroller
#define DTR_OFF      9
#define RTS_ON      11  // used here to signal ISP (in-system-programming) to uC
#define RTS_OFF     12

// telnet state machine states
enum { TN_normal, TN_iac, TN_will, TN_start, TN_end, TN_comPort, TN_setControl };

// process a buffer-full on a telnet connection and return the ending telnet state
static uint8_t ICACHE_FLASH_ATTR
telnetUnwrap(uint8_t *inBuf, int len, uint8_t state)
{
  for (int i=0; i<len; i++) {
    uint8_t c = inBuf[i];
    switch (state) {
    default:
    case TN_normal:
      if (c == IAC) state = TN_iac; // escape char: see what's next
      else uart0_write_char(c);     // regular char
      break;
    case TN_iac:
      switch (c) {
      case IAC:                     // second escape -> write one to outbuf and go normal again
        state = TN_normal;
        uart0_write_char(c);
        break;
      case WILL:                    // negotiation
        state = TN_will;
        break;
      case SB:                      // command sequence begin
        state = TN_start;
        break;
      case SE:                      // command sequence end
        state = TN_normal;
        break;
      default:                      // not sure... let's ignore
        uart0_write_char(IAC);
        uart0_write_char(c);
      }
      break;
    case TN_will:
      state = TN_normal;            // yes, we do COM port options, let's go back to normal
      break;
    case TN_start:                  // in command seq, now comes the type of cmd
      if (c == ComPortOpt) state = TN_comPort;
      else state = TN_end;          // an option we don't know, skip 'til the end seq
      break;
    case TN_end:                    // wait for end seq
      if (c == IAC) state = TN_iac; // simple wait to accept end or next escape seq
      break;
    case TN_comPort:
      if (c == SetControl) state = TN_setControl;
      else state = TN_end;
      break;
    case TN_setControl:             // switch control line and delay a tad
      switch (c) {
      case DTR_ON:
        if (mcu_reset_pin >= 0) {
          os_printf("MCU reset gpio%d\n", mcu_reset_pin);
          GPIO_OUTPUT_SET(mcu_reset_pin, 0);
          os_delay_us(100L);
        } else os_printf("MCU reset: no pin\n");
        break;
      case DTR_OFF:
        if (mcu_reset_pin >= 0) {
          GPIO_OUTPUT_SET(mcu_reset_pin, 1);
          os_delay_us(100L);
        }
        break;
      case RTS_ON:
        if (mcu_isp_pin >= 0) {
          os_printf("MCU ISP gpio%d\n", mcu_isp_pin);
          GPIO_OUTPUT_SET(mcu_isp_pin, 0);
          os_delay_us(100L);
        } else os_printf("MCU isp: no pin\n");
        slip_disabled++;
        break;
      case RTS_OFF:
        if (mcu_isp_pin >= 0) {
          GPIO_OUTPUT_SET(mcu_isp_pin, 1);
          os_delay_us(100L);
        }
        if (slip_disabled > 0) slip_disabled--;
        break;
      }
      state = TN_end;
      break;
    }
  }
  return state;
}

// Generate a reset pulse for the attached microcontroller
void ICACHE_FLASH_ATTR serbridgeReset() {
  if (mcu_reset_pin >= 0) {
    os_printf("MCU reset gpio%d\n", mcu_reset_pin);
    GPIO_OUTPUT_SET(mcu_reset_pin, 0);
    os_delay_us(100L);
    GPIO_OUTPUT_SET(mcu_reset_pin, 1);
  } else os_printf("MCU reset: no pin\n");
}

// Receive callback
static void ICACHE_FLASH_ATTR serbridgeRecvCb(void *arg, char *data, unsigned short len) {
  serbridgeConnData *conn = serbridgeFindConnData(arg);
  //os_printf("Receive callback on conn %p\n", conn);
  if (conn == NULL) return;

  // at the start of a connection we're in cmInit mode and we wait for the first few characters
  // to arrive in order to decide what type of connection this is.. The following if statements
  // do this dispatch. An issue here is that we assume that the first few characters all arrive
  // in the same TCP packet, which is true if the sender is a program, but not necessarily
  // if the sender is a person typing (although in that case the line-oriented TTY input seems
  // to make it work too). If this becomes a problem we need to buffer the first few chars...
  if (conn->conn_mode == cmInit) {

    // If the connection starts with the Arduino or ARM reset sequence we perform a RESET
    if ((len == 2 && strncmp(data, "0 ", 2) == 0) ||
        (len == 2 && strncmp(data, "?\n", 2) == 0) ||
        (len == 3 && strncmp(data, "?\r\n", 3) == 0)) {
      os_printf("MCU Reset=gpio%d ISP=gpio%d\n", mcu_reset_pin, mcu_isp_pin);
      os_delay_us(2*1000L); // time for os_printf to happen
      // send reset to arduino/ARM
      if (mcu_reset_pin >= 0) GPIO_OUTPUT_SET(mcu_reset_pin, 0);
      os_delay_us(100L);
      if (mcu_isp_pin >= 0) GPIO_OUTPUT_SET(mcu_isp_pin, 0);
      os_delay_us(100L);
      if (mcu_reset_pin >= 0) GPIO_OUTPUT_SET(mcu_reset_pin, 1);
      os_delay_us(100L);
      if (mcu_isp_pin >= 0) GPIO_OUTPUT_SET(mcu_isp_pin, 1);
      os_delay_us(1000L);
      conn->conn_mode = cmAVR;
      slip_disabled++; // disable SLIP so it doesn't interfere with flashing

    // If the connection starts with a telnet negotiation we will do telnet
    } else if (len >= 3 && strncmp(data, (char[]){IAC, WILL, ComPortOpt}, 3) == 0) {
      conn->conn_mode = cmTelnet;
      conn->telnet_state = TN_normal;
      // note that the three negotiation chars will be gobbled-up by telnetUnwrap
      os_printf("telnet mode\n");

    // looks like a plain-vanilla connection!
    } else {
      conn->conn_mode = cmTransparent;
    }

  // Process return data on TCP client connections
  } else if (conn->conn_mode == cmTcpClient) {
  }

  // write the buffer to the uart
  if (conn->conn_mode == cmTelnet) {
    conn->telnet_state = telnetUnwrap((uint8_t *)data, len, conn->telnet_state);
  } else {
    uart0_tx_buffer(data, len);
  }

  serledFlash(50); // short blink on serial LED
}

//===== UART -> TCP

// Transmit buffers for the connection pool
static char txbuffer[MAX_CONN][MAX_TXBUFFER];

// Send all data in conn->txbuffer
// returns result from espconn_sent if data in buffer or ESPCONN_OK (0)
// Use only internally from espbuffsend and serbridgeSentCb
static sint8 ICACHE_FLASH_ATTR
sendtxbuffer(serbridgeConnData *conn) {
  sint8 result = ESPCONN_OK;
  if (conn->txbufferlen != 0) {
    //os_printf("%d TX %d\n", system_get_time(), conn->txbufferlen);
    conn->readytosend = false;
    result = espconn_sent(conn->conn, (uint8_t*)conn->txbuffer, conn->txbufferlen);
    conn->txbufferlen = 0;
    if (result != ESPCONN_OK) {
      os_printf("sendtxbuffer: espconn_sent error %d on conn %p\n", result, conn);
    }
  }
  return result;
}

// espbuffsend adds data to the send buffer. If the previous send was completed it calls
// sendtxbuffer and espconn_sent.
// Returns ESPCONN_OK (0) for success, -128 if buffer is full or error from  espconn_sent
// Use espbuffsend instead of espconn_sent as it solves the problem that espconn_sent must
// only be called *after* receiving an espconn_sent_callback for the previous packet.
static sint8 ICACHE_FLASH_ATTR
espbuffsend(serbridgeConnData *conn, const char *data, uint16 len) {
  if (conn->txbufferlen + len > MAX_TXBUFFER) {
    os_printf("espbuffsend: txbuffer full on conn %p\n", conn);
    return -128;
  }
  os_memcpy(conn->txbuffer + conn->txbufferlen, data, len);
  conn->txbufferlen += len;
  if (conn->readytosend) {
    return sendtxbuffer(conn);
  } else {
    //os_printf("%d QU %d\n", system_get_time(), conn->txbufferlen);
  }
  return ESPCONN_OK;
}

void ICACHE_FLASH_ATTR
console_process(char *buf, short len) {
  // push buffer into web-console
  for (short i=0; i<len; i++)
    console_write_char(buf[i]);
  // push the buffer into each open connection
  for (short i=0; i<MAX_CONN; i++) {
    if (connData[i].conn && connData[i].conn_mode != cmTcpClient) {
      espbuffsend(&connData[i], buf, len);
    }
  }
}

//callback after the data are sent
static void ICACHE_FLASH_ATTR
serbridgeSentCb(void *arg) {
  serbridgeConnData *conn = serbridgeFindConnData(arg);
  //os_printf("Sent callback on conn %p\n", conn);
  if (conn == NULL) return;
  //os_printf("%d ST\n", system_get_time());
  conn->readytosend = true;
  sendtxbuffer(conn); // send possible new data in txbuffer
}

//===== Connect / disconnect

// Error callback (it's really poorly named, it's not a "connection reconnected" callback,
// it's really a "connection broken, please reconnect" callback)
static void ICACHE_FLASH_ATTR serbridgeReconCb(void *arg, sint8 err) {
  serbridgeConnData *sbConn = serbridgeFindConnData(arg);
  if (sbConn == NULL) return;
  if (sbConn->conn_mode == cmAVR) {
    if (slip_disabled > 0) slip_disabled--;
  }
  // Close the connection
  espconn_disconnect(sbConn->conn);
  // free connection slot
  sbConn->conn = NULL;
}

// Disconnection callback
static void ICACHE_FLASH_ATTR serbridgeDisconCb(void *arg) {
  serbridgeConnData *sbConn = serbridgeFindConnData(arg);
  if (sbConn == NULL) return;
  // send reset to arduino/ARM
  if (sbConn->conn_mode == cmAVR) {
    if (slip_disabled > 0) slip_disabled--;
    if (mcu_reset_pin >= 0) {
      GPIO_OUTPUT_SET(mcu_reset_pin, 0);
      os_delay_us(100L);
      GPIO_OUTPUT_SET(mcu_reset_pin, 1);
    }
  }
  // free connection slot
  sbConn->conn = NULL;
}

// New connection callback, use one of the connection descriptors, if we have one left.
static void ICACHE_FLASH_ATTR serbridgeConnectCb(void *arg) {
  struct espconn *conn = arg;
  // Find empty conndata in pool
  int i;
  for (i=0; i<MAX_CONN; i++) if (connData[i].conn==NULL) break;
  os_printf("Accept port 23, conn=%p, pool slot %d\n", conn, i);

  if (i==MAX_CONN) {
    os_printf("Aiee, conn pool overflow!\n");
    espconn_disconnect(conn);
    return;
  }

  conn->reverse = connData+i;
  connData[i].conn = conn;
  connData[i].txbufferlen = 0;
  connData[i].readytosend = true;
  connData[i].telnet_state = 0;
  connData[i].conn_mode = cmInit;

  espconn_regist_recvcb(conn, serbridgeRecvCb);
  espconn_regist_reconcb(conn, serbridgeReconCb);
  espconn_regist_disconcb(conn, serbridgeDisconCb);
  espconn_regist_sentcb(conn, serbridgeSentCb);

  espconn_set_opt(conn, ESPCONN_REUSEADDR|ESPCONN_NODELAY);
}

//===== Initialization

void ICACHE_FLASH_ATTR serbridgeInitPins() {
  mcu_reset_pin = flashConfig.reset_pin;
  mcu_isp_pin = flashConfig.isp_pin;
  os_printf("Serbridge pins: reset=%d isp=%d swap=%d\n",
      mcu_reset_pin, mcu_isp_pin, flashConfig.swap_uart);

  if (flashConfig.swap_uart) {
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, 4);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, 4);
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_MTCK_U);
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_MTDO_U);
    system_uart_swap();
  } else {
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, 0);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, 0);
    system_uart_de_swap();
  }

  // set both pins to 1 before turning them on so we don't cause a reset
  if (mcu_isp_pin >= 0)   GPIO_OUTPUT_SET(mcu_isp_pin, 1);
  if (mcu_reset_pin >= 0) GPIO_OUTPUT_SET(mcu_reset_pin, 1);
  // switch pin mux to make these pins GPIO pins
  if (mcu_reset_pin >= 0) makeGpio(mcu_reset_pin);
  if (mcu_isp_pin >= 0)   makeGpio(mcu_isp_pin);
}

// Start transparent serial bridge TCP server on specified port (typ. 23)
void ICACHE_FLASH_ATTR serbridgeInit(int port) {
  serbridgeInitPins();

  int i;
  for (i = 0; i < MAX_CONN; i++) {
    connData[i].conn = NULL;
    connData[i].txbuffer = txbuffer[i];
  }
  serbridgeConn.type = ESPCONN_TCP;
  serbridgeConn.state = ESPCONN_NONE;
  serbridgeTcp.local_port = port;
  serbridgeConn.proto.tcp = &serbridgeTcp;

  espconn_regist_connectcb(&serbridgeConn, serbridgeConnectCb);
  espconn_accept(&serbridgeConn);
  espconn_tcp_set_max_con_allow(&serbridgeConn, MAX_CONN);
  espconn_regist_time(&serbridgeConn, SER_BRIDGE_TIMEOUT, 0);
}
