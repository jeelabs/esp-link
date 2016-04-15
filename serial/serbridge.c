// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt

/*  Modified by Christophe Duparquet: extended implementation of RFC 2217
 *
 *  Verified on ESP-WROOM-02 with the following configuration:
 *    Reset:      gpio5
 *    ISP/Flash:  disabled
 *    Conn LED:   gpio4
 *    Serial LED: disabled
 *    UART pins:  swapped
 *    RX pull-up: yes
 *
 *    ESP connections:
 *      GPIO2 (#7) / 3V3 (#1) -> 1kR
 *      GPIO0 (#8) / 3V3 (#1) -> 1kR
 *      GPIO0 (#8) = USB serial RTS
 *      GPIO13 (#5) / GPIO15 (#6) -> 1kR
 *      GPIO15 (#6) / GND (#9) -> 10kR
 *      EN (#2) / 3V3 (#1) -> 10 kR
 *      #18 = #13 = #1 = GND
 *      RST (#15) -> USB serial RTS
 *
 *    ATtiny85 (3V3) connections:
 *      RESET (#1) -> ESP pin GPIO5 (#14)
 *      RXTX  (#2) -> ESP pin GPIO13 (#5)
 *
 *    Command: diabolo -t rfc2217://192.168.1.78:23
 */

#include "esp8266.h"

#include "uart.h"
#include "crc16.h"
#include "serbridge.h"
#include "serled.h"
#include "config.h"
#include "console.h"
#include "slip.h"
#include "cmd.h"
#include "syslog.h"

#define SKIP_AT_RESET


/*  ESP8266
 */
#define IROM			ICACHE_FLASH_ATTR
#define REG_BRR			(*(volatile uint32_t*)0x60000014)
#define REG_CONF0		(*(volatile uint32_t*)0x60000020)
#define REG_CONF1		(*(volatile uint32_t*)0x60000024)


static struct espconn serbridgeConn1; // plain bridging port
static struct espconn serbridgeConn2; // programming port
static esp_tcp serbridgeTcp1, serbridgeTcp2;
static int8_t mcu_reset_pin, mcu_isp_pin;

extern uint8_t slip_disabled;   // disable slip to allow flashing of attached MCU

void (*programmingCB)(char *buffer, short length) = NULL;

// Connection pool
serbridgeConnData connData[MAX_CONN];

static sint8 IROM	espbuffsend(serbridgeConnData *conn, const char *data, uint16 len) ;
static sint8 IROM	espbuffsend_tn(serbridgeConnData *conn, const char *data, uint16 len) ;


// Telnet protocol (RFC854) characters
//
#define IAC			0xFF	// escape
#define DONT			0xFE	//   negociation (RFC855)
#define DO			0xFD	//   negociation
#define WONT			0xFC	//   negociation
#define WILL			0xFB	//   negociation
#define SB			0xFA	//     subnegotiation begin
#define SE			0xF0	//     subnegotiation end

#define BINARY			0x00	// RFC856
#define ECHO			0x01	// RFC857
#define SUPPRESS_GO_AHEAD	0x03	// RFC858

#define COM_PORT_OPTION		0x2C	// RFC2217
#define SET_BAUDRATE		1	//   suboption "BAUDRATE"
#define SET_DATASIZE		2	//   suboption "DATASIZE"
#define SET_PARITY		3	//   suboption "PARITY"
#define SET_STOPSIZE		4	//   suboption "STOPSIZE"
#define SET_CONTROL		5	//   suboption "CONTROL"
#define PURGE_DATA		12	//   suboption "PURGE"


#ifdef SERBR_DBG
#  define dbgf(...)	do { os_printf(__VA_ARGS__); }while(0)
#else
#  define dbgf(...)	do{}while(0)
#endif


/*  Telnet state machine states
 */
enum {
  ST_NORMAL,
  ST_IAC,
  ST_NEGO,
  ST_SB,
  ST_COMPORT,
  ST_BAUDRATE,
  ST_DATASIZE,
  ST_PARITY,
  ST_STOPSIZE,
  ST_CONTROL,
  ST_PURGE,
  ST_WAIT_IAC
};


/*  Telnet state machine: process one byte comming from a telnet connection
 */
static void IROM telnet_process_char ( serbridgeConnData *conn, char c )
{
#define state		conn->tn_state
#define opt		conn->tn_opt
#define vlen		conn->tn_vlen
#define value		conn->tn_value

  if ( state == ST_NORMAL ) {
    if ( c == IAC )
      state = ST_IAC ;
    else
      uart0_write_char(c) ;
    return ;
  }

  if ( state == ST_IAC ) {
    if ( c == IAC ) {
      /*
       *  Dual IAC means char \xFF
       */
      uart0_write_char(0xFF);
      state = ST_NORMAL ;
      return ;
    }
    else if ( c == DONT || c == DO || c == WONT || c == WILL ) {
      /*
       *  Beginning of negociation
       */
      opt = c ;
      state = ST_NEGO ;
      return ;
    }
    else if ( c == SB ) {
      /*
       *  Beginning of sub option
       */
      opt = c ;
      state = ST_SB ;
      return ;
    }
    else if ( c == SE ) {
      /*
       *  End of sub option
       */
      opt = c ;
      state = ST_NORMAL ;
      return ;
    }
    return ;
  }

  if ( state == ST_NEGO ) {
    dbgf("TELNET: IAC NEGO (%02X)", c);
    if ( c == BINARY ||
	 c == ECHO ||
	 c == SUPPRESS_GO_AHEAD ||
	 c == COM_PORT_OPTION ) {
      /*
       *  Acknowledge positive requests for known options
       */
      dbgf(": OK\n");
      if ( opt == DO )
	opt = WILL ;
      else if ( opt == WILL )
	opt = DO ;
    }
    else {
      /*
       *  Deny positive requests for unknown options
       */
      dbgf(": REJECTED\n");
      if ( opt == DO )
	opt = WONT ;
      else if ( opt == WILL )
	opt = DONT ;
    }
    /*
     *  Send reply
     */
    espbuffsend( conn, (char[]){IAC,opt,c}, 3 );
    state = ST_NORMAL ;
    return ;
  }

  if ( state == ST_SB ) {
    if ( c == COM_PORT_OPTION )
      state = ST_COMPORT ;
    else
      state = ST_WAIT_IAC ;
    return ;
  }

  if ( state == ST_WAIT_IAC ) {
    if ( c == IAC )
      state = ST_IAC ;
    return ;
  }

  if ( state == ST_COMPORT ) {
    if ( c == SET_BAUDRATE ) {
      dbgf("  BAUDRATE:");
      vlen = 0 ;
      state = ST_BAUDRATE ;
    } else if ( c == SET_DATASIZE ) {
      dbgf("  DATASIZE:");
      state = ST_DATASIZE ;
    } else if ( c == SET_PARITY ) {
      dbgf("  PARITY:");
      state = ST_PARITY ;
    } else if ( c == SET_STOPSIZE ) {
      dbgf("  STOPSIZE:");
      state = ST_STOPSIZE ;
    } else if ( c == SET_CONTROL ) {
      dbgf("  CONTROL:");
      state = ST_CONTROL ;
    } else if ( c == PURGE_DATA ) {
      dbgf("  PURGE:");
      state = ST_PURGE ;
    } else {
      dbgf("UNKNOWN: %02X\n", c);
      state = ST_WAIT_IAC ;
    }
    return ;
  }

  if ( state == ST_BAUDRATE ) {
    /*
     *  Get 4 bytes of baudrate (MSB first)
     */
    dbgf(" %02X", c);

    value <<= 8 ;
    value += c ;
    vlen++ ;
    if ( vlen == 4 ) {
      /*
       *  4 bytes received, process the value
       *
       *  Note: must acknowledge with the value that was set otherwise the
       *  pyserial client considers that the set value is rejected.
       */
      if ( value == 0 )
	/*
	 *  Get actual baudrate
	 */
	value = (int)(0.5 + 80e6/REG_BRR);
      else
	/*
	 *  Set baudrate
	 */
	REG_BRR = (int)(0.5 + 80e6/value);

      /*  Acknowledge
       */
      dbgf(" = %ld\n", value);
      char v0 = value ;
      char v1 = value >> 8 ;
      char v2 = value >> 16 ;
      char v3 = value >> 24 ;
      espbuffsend( conn, (char[]){IAC,SB,COM_PORT_OPTION,100+SET_BAUDRATE}, 4 );
      espbuffsend_tn( conn, (char[]){v3,v2,v1,v0}, 4 );
      espbuffsend( conn, (char[]){IAC,SE}, 2 );
      state = ST_WAIT_IAC ;
      return ;
    }
    return ;
  }

  if ( state == ST_DATASIZE ) {
    if ( c >= 5 && c <= 8 ) {
      /*
       *  Set data size
       *  Store databits in bits 3..2 of register conf0
       */
      REG_CONF0 = (REG_CONF0 & ~0xC) | ((c-5)<<2) ;
    }
    else {
      /*
       *  Get data size
       */
      c = 5 + ((REG_CONF0>>2) & 0x03) ;
    }

    /*  Acknowledge datasize
     */
    dbgf(" %d\n", c );
    espbuffsend( conn, (char[]){IAC,SB,COM_PORT_OPTION,100+SET_DATASIZE,c,IAC,SE},7 );
    state = ST_WAIT_IAC ;
    return ;
  }

  if ( state == ST_PARITY ) {
    if ( c == 0 ) {
      /*
       *  Get actual parity: CONF0 bits 1..0
       */
      c = (REG_CONF0>>2) & 0x03 ;
      if ( c==2 )
	c = 1 ;
      else if ( c==3 )
	c = 2 ;
      else
	c = 3 ;
    }
    else if ( c==1 || c==2 || c==3 ) {
      /*
       *  Set parity
       *    1  NONE
       *    2  ODD
       *    3  EVEN
       */
      char d ;
      if ( c==1 )
	d = 0 ;
      else if ( c==2 )
	d = 3 ;
      else
	d = 2 ;
      REG_CONF0 = (REG_CONF0 & ~0x3) | d ;
    }
    else {
      /*
       *  Do not acknowledge unknown parity value
       */
      state = ST_WAIT_IAC ;
      return ;
    }

    /*  Acknowledge parity
     */
    dbgf(" %d\n", c );
    espbuffsend( conn, (char[]){IAC,SB,COM_PORT_OPTION,100+SET_PARITY,c,IAC,SE},7 );
    state = ST_WAIT_IAC ;
    return ;
  }

  if ( state == ST_STOPSIZE ) {
    if ( c == 0 ) {
      /*
       *  Get actual stop bits: CONF0 bits 5..4
       */
      c = REG_CONF0>>4 & 0x03 ;
      if ( c==2 )
	c = 3 ;
      else if ( c==3 )
	c = 2 ;
    }
    else if ( c >= 1 && c <= 3 ) {
      /*
       *  Set stop bits
       *    1  1 bit
       *    2  2 bits
       *    3  1.5 bit
       */
      char d = c ;
      if ( c==2 )
	d = 3 ;
      else if ( c==3 )
	d = 2 ;
      REG_CONF0 = (REG_CONF0 & ~0x30) | (d<<4) ;
    }
    else {
      /*
       *  Do not acknowledge unknown stop bits value
       */
      state = ST_WAIT_IAC ;
      return ;
    }

    /*  Acknowledge stop bits
     */
    dbgf(" %d\n", c );
    espbuffsend( conn, (char[]){IAC,SB,COM_PORT_OPTION,100+SET_STOPSIZE,c,IAC,SE},7 );
    state = ST_WAIT_IAC ;
    return ;
  }

  if ( state == ST_CONTROL ) {
    if ( c == 1 ) {
      /*
       *  Use No Flow Control (outbound/both)
       *
       *  Disable TX hardware flow: CONF0 bit 15 = 0
       *  Disable RX hardware flow: CONF1 bit 23 = 0
       */
      REG_CONF0 &= ~(1ULL<<15) ;
      REG_CONF1 &= ~(1ULL<<23) ;
    }
    else if ( c == 5 ) {
      /*
       *  Set BREAK State ON
       */
      REG_CONF0 |= (1ULL<<8) ;
    }
    else if ( c == 6 ) {
      /*
       *  Set BREAK State OFF
       */
      REG_CONF0 &= ~(1ULL<<8) ;
    }
    else if ( c == 8 ) {
#ifdef USE_UART_CONTROL_LINES
      /*
       *  Set DTR Signal State ON
       *
       *  Assert DTR: CONF0 bit 7
       */
      REG_CONF0 |= (1ULL<<7) ;
#else
      /*
       *  Set DTR Signal State ON
       *
       *  Drive MCU reset LOW
       */
      if (mcu_reset_pin >= 0) {
	dbgf("MCU reset gpio%d\n", mcu_reset_pin);
	GPIO_OUTPUT_SET(mcu_reset_pin, 0);
      }
      else
	dbgf("MCU reset: no pin\n");
#endif
    }
    else if ( c == 9 ) {
#ifdef USE_UART_CONTROL_LINES
      /*
       *  Set DTR Signal State OFF
       *
       *  Assert DTR: CONF0 bit 7
       */
      REG_CONF0 &= ~(1ULL<<7) ;
#else
      /*
       *  Set DTR Signal State OFF
       *
       *  Drive MCU reset HIGH
       */
      if (mcu_reset_pin >= 0) {
	dbgf("MCU reset gpio%d\n", mcu_reset_pin);
	GPIO_OUTPUT_SET(mcu_reset_pin, 1);
      }
      else
	dbgf("MCU reset: no pin\n");
#endif
    }
    else if ( c == 11 ) {
#ifdef USE_UART_CONTROL_LINES
      /*
       *  Set RTS Signal State ON
       *
       *  Assert RTS: CONF0 bit 6 = 1
       */
      REG_CONF0 |= (1ULL<<6) ;
#else
      /*
       *  Set RTS Signal State ON
       *
       *  Drive MCU ISP pin LOW
       */
      if (mcu_isp_pin >= 0) {
	dbgf("MCU ISP gpio%d\n", mcu_isp_pin);
	GPIO_OUTPUT_SET(mcu_isp_pin, 0);
	os_delay_us(100L);
      }
      else
	dbgf("MCU isp: no pin\n");
      slip_disabled++;
#endif
    }
    else if ( c == 12 ) {
#ifdef USE_UART_CONTROL_LINES
      /*
       *  Set RTS Signal State OFF
       *
       *  Assert RTS: CONF0 bit 6 = 0
       */
      REG_CONF0 &= ~(1ULL<<6) ;
#else
      /*
       *  Set RTS Signal State OFF
       *
       *  Drive MCU ISP pin HIGH
       */
      if (mcu_isp_pin >= 0) {
	GPIO_OUTPUT_SET(mcu_isp_pin, 1);
	os_delay_us(100L);
      }
      if (slip_disabled > 0) slip_disabled--;
#endif
    }
    else {
      /*
       *  Do not acknowledge unknown control
       */
      state = ST_WAIT_IAC ;
      return ;
    }

    /*  Acknowledge control
     */
    dbgf(" %d\n", c );
    espbuffsend( conn, (char[]){IAC,SB,COM_PORT_OPTION,100+SET_CONTROL,c,IAC,SE},7 );
    state = ST_WAIT_IAC ;
    return ;
  }

  if ( state == ST_PURGE ) {
    if ( c == 1 ) {
      /*
       *  Purge access server receive data buffer
       *
       *  Reset RX FIFO: CONF0 bit 17 = 1
       */
      //	REG_CONF0 |= (1ULL<<17) ;
    }
    else if ( c == 2 ) {
      /*
       *  Purge access server transmit data buffer
       *
       *  Reset TX FIFO: CONF0 bit 18 = 1
       */
      //	REG_CONF0 |= (1ULL<<18) ;
    }
    else {
      state = ST_WAIT_IAC ;
      return ;
    }

    /*  Acknowledge purge
     */
    dbgf(" %d\n", c );
    espbuffsend( conn, (char[]){IAC,SB,COM_PORT_OPTION,100+PURGE_DATA,c,IAC,SE},7 );
    state = ST_WAIT_IAC ;
    return ;
  }

  /*  Unexpected char
   */
  state = ST_WAIT_IAC ;

#undef state
#undef opt
#undef vlen
#undef value
}


/*  Process bytes comming from a telnet connection
 */
static void IROM telnet_process_buf ( serbridgeConnData *conn, uint8_t *buf, int len )
{
#ifdef SERBR_DBG
  os_printf("TELNET:");
  for ( int i=0; i<len; i++ ) {
    os_printf(" %02X", buf[i]);
  }
  os_printf(" \n");
#endif

  for ( int i=0 ; i<len ; i++ )
    telnet_process_char( conn, buf[i] );
}


// Generate a reset pulse for the attached microcontroller
void ICACHE_FLASH_ATTR
serbridgeReset()
{
  if (mcu_reset_pin >= 0) {
#ifdef SERBR_DBG
    os_printf("MCU reset gpio%d\n", mcu_reset_pin);
#endif
    GPIO_OUTPUT_SET(mcu_reset_pin, 0);
    os_delay_us(2000L); // esp8266 needs at least 1ms reset pulse, it seems...
    GPIO_OUTPUT_SET(mcu_reset_pin, 1);
  }
#ifdef SERBR_DBG
  else { os_printf("MCU reset: no pin\n"); }
#endif
}

// Receive callback
static void ICACHE_FLASH_ATTR
serbridgeRecvCb(void *arg, char *data, unsigned short len)
{
  serbridgeConnData *conn = ((struct espconn*)arg)->reverse;
  //os_printf("Receive callback on conn %p\n", conn);
  if (conn == NULL) return;

  bool startPGM = false;

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
      startPGM = true;
      conn->conn_mode = cmPGM;

      // If the connection starts with a telnet negotiation we will do telnet
    }
    //    else if (len >= 3 && strncmp(data, (char[]){IAC, WILL, COM_PORT_OPTION}, 3) == 0) {
    else if ( len>2 && data[0]==IAC && (data[1]==WILL || data[1]==DO) ) {
      conn->conn_mode = cmTelnet;
      conn->tn_state = ST_NORMAL;
      // note that the three negotiation chars will be gobbled-up by telnetUnwrap
#ifdef SERBR_DBG
      os_printf("telnet mode\n");
#endif

      // looks like a plain-vanilla connection!
    }
    else {
      conn->conn_mode = cmTransparent;
    }

    // if we start out in cmPGM mode due to a connection to the second port we need to do the
    // reset dance right away
  } else if (conn->conn_mode == cmPGMInit) {
    conn->conn_mode = cmPGM;
    startPGM = true;
  }

  // do the programming reset dance
  if (startPGM) {
#ifdef SERBR_DBG
    os_printf("MCU Reset=gpio%d ISP=gpio%d\n", mcu_reset_pin, mcu_isp_pin);
    os_delay_us(2*1000L); // time for os_printf to happen
#endif
    // send reset to arduino/ARM, send "ISP" signal for the duration of the programming
    if (mcu_reset_pin >= 0) GPIO_OUTPUT_SET(mcu_reset_pin, 0);
    os_delay_us(100L);
    if (mcu_isp_pin >= 0) GPIO_OUTPUT_SET(mcu_isp_pin, 0);
    os_delay_us(2000L);
    if (mcu_reset_pin >= 0) GPIO_OUTPUT_SET(mcu_reset_pin, 1);
    //os_delay_us(100L);
    //if (mcu_isp_pin >= 0) GPIO_OUTPUT_SET(mcu_isp_pin, 1);
    os_delay_us(1000L); // wait a millisecond before writing to the UART below
    conn->conn_mode = cmPGM;
    slip_disabled++; // disable SLIP so it doesn't interfere with flashing
#ifdef SKIP_AT_RESET
    serledFlash(50); // short blink on serial LED
    return;
#endif
  }

  if ( conn->conn_mode == cmTelnet ) {
    /*
     *  Process Telnet protocol
     */
    telnet_process_buf( conn, (uint8_t *)data, len );
  } else {
    // write the buffer to the uart
    uart0_tx_buffer( data, len );
  }

  serledFlash(50); // short blink on serial LED
}

//===== UART -> TCP

// Send all data in conn->txbuffer
// returns result from espconn_sent if data in buffer or ESPCONN_OK (0)
// Use only internally from espbuffsend and serbridgeSentCb
static sint8 ICACHE_FLASH_ATTR
sendtxbuffer(serbridgeConnData *conn)
{
  sint8 result = ESPCONN_OK;
  if (conn->txbufferlen != 0) {
    //os_printf("TX %p %d\n", conn, conn->txbufferlen);
    conn->readytosend = false;
    result = espconn_sent(conn->conn, (uint8_t*)conn->txbuffer, conn->txbufferlen);
    conn->txbufferlen = 0;
    if (result != ESPCONN_OK) {
      os_printf("sendtxbuffer: espconn_sent error %d on conn %p\n", result, conn);
      conn->txbufferlen = 0;
      if (!conn->txoverflow_at) conn->txoverflow_at = system_get_time();
    } else {
      conn->sentbuffer = conn->txbuffer;
      conn->txbuffer = NULL;
      conn->txbufferlen = 0;
    }
  }
  return result;
}


//  Escape '\xFF' bytes in buffer for Telnet protocol before sending it over the
//  air
//
static sint8 ICACHE_FLASH_ATTR
espbuffsend_tn ( serbridgeConnData *conn, const char *data, uint16 len )
{
  //  How many bytes for the new buffer?
  //
  int n = 0 ;
  for ( int i=0 ; i<len ; i++ )
    if ( data[i] == 0xFF )
      n++ ;

  //  Allocate and populate new buffer if necessary
  //
  if ( n ) {
    char *tnbuf = os_zalloc(len+n);
    if ( tnbuf == NULL ) {
      os_printf("espbuffsend: could not allocate buffer for telnet escape\n");
      return -128;
    }

    n = 0 ;
    for ( int i=0 ; i<len ; i++ ) {
      tnbuf[n++] = data[i] ;
      if ( data[i] == 0xFF )
	tnbuf[n++] = 0xFF ;
    }

    //  Send buffer
    //
    sint8 result = espbuffsend( conn, tnbuf, n );
    os_free( tnbuf );
    return result ;
  }
  
  return espbuffsend( conn, data, len );
}


// espbuffsend adds data to the send buffer. If the previous send was completed it calls
// sendtxbuffer and espconn_sent.
// Returns ESPCONN_OK (0) for success, -128 if buffer is full or error from  espconn_sent
// Use espbuffsend instead of espconn_sent as it solves the problem that espconn_sent must
// only be called *after* receiving an espconn_sent_callback for the previous packet.
static sint8 ICACHE_FLASH_ATTR
espbuffsend(serbridgeConnData *conn, const char *data, uint16 len )
{
  if (conn->txbufferlen >= MAX_TXBUFFER) goto overflow;

  // make sure we indeed have a buffer
  if (conn->txbuffer == NULL) conn->txbuffer = os_zalloc(MAX_TXBUFFER);
  if (conn->txbuffer == NULL) {
    os_printf("espbuffsend: cannot alloc tx buffer\n");
    return -128;
  }

  // add to send buffer
  uint16_t avail = conn->txbufferlen+len > MAX_TXBUFFER ? MAX_TXBUFFER-conn->txbufferlen : len;
  os_memcpy(conn->txbuffer + conn->txbufferlen, data, avail);
  conn->txbufferlen += avail;

  // try to send
  sint8 result = ESPCONN_OK;
  if (conn->readytosend) result = sendtxbuffer(conn);

  if (avail < len) {
    // some data didn't fit into the buffer
    if (conn->txbufferlen == 0) {
      // we sent the prior buffer, so try again
      return espbuffsend(conn, data+avail, len-avail);
    }
    goto overflow;
  }
  return result;

 overflow:
  if (conn->txoverflow_at) {
    // we've already been overflowing
    if (system_get_time() - conn->txoverflow_at > 10*1000*1000) {
      // no progress in 10 seconds, kill the connection
      os_printf("serbridge: killing overlowing stuck conn %p\n", conn);
      espconn_disconnect(conn->conn);
    }
    // else be silent, we already printed an error
  } else {
    // print 1-time message and take timestamp
    os_printf("serbridge: txbuffer full, conn %p\n", conn);
    conn->txoverflow_at = system_get_time();
  }
  return -128;
}

//callback after the data are sent
static void ICACHE_FLASH_ATTR
serbridgeSentCb(void *arg)
{
  serbridgeConnData *conn = ((struct espconn*)arg)->reverse;
  //os_printf("Sent CB %p\n", conn);
  if (conn == NULL) return;
  //os_printf("%d ST\n", system_get_time());
  if (conn->sentbuffer != NULL) os_free(conn->sentbuffer);
  conn->sentbuffer = NULL;
  conn->readytosend = true;
  conn->txoverflow_at = 0;
  sendtxbuffer(conn); // send possible new data in txbuffer
}

void ICACHE_FLASH_ATTR
console_process(char *buf, short len)
{
  // push buffer into web-console
  for (short i=0; i<len; i++)
    console_write_char(buf[i]);
  // push the buffer into each open connection
  for (short i=0; i<MAX_CONN; i++) {
    if (connData[i].conn) {
      if ( connData[i].conn_mode == cmTelnet )
	espbuffsend_tn( &connData[i], buf, len );
      else
	espbuffsend( &connData[i], buf, len );
    }
  }
}


// callback with a buffer of characters that have arrived on the uart
void ICACHE_FLASH_ATTR
serbridgeUartCb(char *buf, short length)
{
  if (programmingCB) {
    programmingCB(buf, length);
  } else if (!flashConfig.slip_enable || slip_disabled > 0) {
    //os_printf("SLIP: disabled got %d\n", length);
    console_process(buf, length);
  } else {
    slip_parse_buf(buf, length);
  }

  serledFlash(50); // short blink on serial LED
}

//===== Connect / disconnect

// Disconnection callback
static void ICACHE_FLASH_ATTR
serbridgeDisconCb(void *arg)
{
  serbridgeConnData *conn = ((struct espconn*)arg)->reverse;
  if (conn == NULL) return;
  // Free buffers
  if (conn->sentbuffer != NULL) os_free(conn->sentbuffer);
  conn->sentbuffer = NULL;
  if (conn->txbuffer != NULL) os_free(conn->txbuffer);
  conn->txbuffer = NULL;
  conn->txbufferlen = 0;
  // Send reset to attached uC if it was in programming mode
  if (conn->conn_mode == cmPGM && mcu_reset_pin >= 0) {
    if (mcu_isp_pin >= 0) GPIO_OUTPUT_SET(mcu_isp_pin, 1);
    os_delay_us(100L);
    GPIO_OUTPUT_SET(mcu_reset_pin, 0);
    os_delay_us(100L);
    GPIO_OUTPUT_SET(mcu_reset_pin, 1);
  }
  conn->conn = NULL;
}

// Connection reset callback (note that there will be no DisconCb)
static void ICACHE_FLASH_ATTR
serbridgeResetCb(void *arg, sint8 err)
{
  os_printf("serbridge: connection reset err=%d\n", err);
  serbridgeDisconCb(arg);
}

// New connection callback, use one of the connection descriptors, if we have one left.
static void ICACHE_FLASH_ATTR
serbridgeConnectCb(void *arg)
{
  struct espconn *conn = arg;
  // Find empty conndata in pool
  int i;
  for (i=0; i<MAX_CONN; i++) if (connData[i].conn==NULL) break;
#ifdef SERBR_DBG
  os_printf("Accept port %d, conn=%p, pool slot %d\n", conn->proto.tcp->local_port, conn, i);
#endif
  syslog(SYSLOG_FAC_USER, SYSLOG_PRIO_NOTICE, "esp-link", "Accept port %d, conn=%p, pool slot %d\n", conn->proto.tcp->local_port, conn, i);
  if (i==MAX_CONN) {
#ifdef SERBR_DBG
    os_printf("Aiee, conn pool overflow!\n");
#endif
	syslog(SYSLOG_FAC_USER, SYSLOG_PRIO_WARNING, "esp-link", "Aiee, conn pool overflow!\n");
    espconn_disconnect(conn);
    return;
  }

  os_memset(connData+i, 0, sizeof(struct serbridgeConnData));
  connData[i].conn = conn;
  conn->reverse = connData+i;
  connData[i].readytosend = true;
  connData[i].conn_mode = cmInit;
  // if it's the second port we start out in programming mode
  if (conn->proto.tcp->local_port == serbridgeConn2.proto.tcp->local_port)
    connData[i].conn_mode = cmPGMInit;

  espconn_regist_recvcb(conn, serbridgeRecvCb);
  espconn_regist_disconcb(conn, serbridgeDisconCb);
  espconn_regist_reconcb(conn, serbridgeResetCb);
  espconn_regist_sentcb(conn, serbridgeSentCb);

  espconn_set_opt(conn, ESPCONN_REUSEADDR|ESPCONN_NODELAY);
}

//===== Initialization

void ICACHE_FLASH_ATTR
serbridgeInitPins()
{
  mcu_reset_pin = flashConfig.reset_pin;
  mcu_isp_pin = flashConfig.isp_pin;
#ifdef SERBR_DBG
  os_printf("Serbridge pins: reset=%d isp=%d swap=%d\n",
	    mcu_reset_pin, mcu_isp_pin, flashConfig.swap_uart);
#endif

  if (flashConfig.swap_uart) {
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, 4); // RX
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, 4); // TX
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_MTDO_U);
    if (flashConfig.rx_pullup) PIN_PULLUP_EN(PERIPHS_IO_MUX_MTCK_U);
    else                       PIN_PULLUP_DIS(PERIPHS_IO_MUX_MTCK_U);
    system_uart_swap();
  } else {
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, 0);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, 0);
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
    if (flashConfig.rx_pullup) PIN_PULLUP_EN(PERIPHS_IO_MUX_U0RXD_U);
    else                       PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0RXD_U);
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
void ICACHE_FLASH_ATTR
serbridgeInit(int port1, int port2)
{
  serbridgeInitPins();

  os_memset(connData, 0, sizeof(connData));
  os_memset(&serbridgeTcp1, 0, sizeof(serbridgeTcp1));
  os_memset(&serbridgeTcp2, 0, sizeof(serbridgeTcp2));

  // set-up the primary port for plain bridging
  serbridgeConn1.type = ESPCONN_TCP;
  serbridgeConn1.state = ESPCONN_NONE;
  serbridgeTcp1.local_port = port1;
  serbridgeConn1.proto.tcp = &serbridgeTcp1;

  espconn_regist_connectcb(&serbridgeConn1, serbridgeConnectCb);
  espconn_accept(&serbridgeConn1);
  espconn_tcp_set_max_con_allow(&serbridgeConn1, MAX_CONN);
  espconn_regist_time(&serbridgeConn1, SER_BRIDGE_TIMEOUT, 0);

  // set-up the secondary port for programming
  serbridgeConn2.type = ESPCONN_TCP;
  serbridgeConn2.state = ESPCONN_NONE;
  serbridgeTcp2.local_port = port2;
  serbridgeConn2.proto.tcp = &serbridgeTcp2;

  espconn_regist_connectcb(&serbridgeConn2, serbridgeConnectCb);
  espconn_accept(&serbridgeConn2);
  espconn_tcp_set_max_con_allow(&serbridgeConn2, MAX_CONN);
  espconn_regist_time(&serbridgeConn2, SER_BRIDGE_TIMEOUT, 0);
}
