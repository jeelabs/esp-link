/*
 * File : uart.c
 * This file is part of Espressif's AT+ command set program.
 * Copyright (C) 2013 - 2016, Espressif Systems
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------------
 * Heavily modified and enhanced by Thorsten von Eicken in 2015
 */
#include "esp8266.h"
#include "task.h"
#include "uart.h"

#ifdef UART_DBG
#define DBG_UART(format, ...) os_printf(format, ## __VA_ARGS__)
#else
#define DBG_UART(format, ...) do { } while(0)
#endif

LOCAL uint8_t uart_recvTaskNum;

// UartDev is defined and initialized in rom code.
extern UartDevice    UartDev;
#define MAX_CB 4
static UartRecv_cb uart_recv_cb[4];

static void uart0_rx_intr_handler(void *para);

/******************************************************************************
 * FunctionName : uart_config
 * Description  : Internal used function
 *                UART0 used for data TX/RX, RX buffer size is 0x100, interrupt enabled
 *                UART1 just used for debug output
 * Parameters   : uart_no, use UART0 or UART1 defined ahead
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart_config(uint8 uart_no, UartBautRate baudrate, uint32 conf0)
{
  if (uart_no == UART1) {
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO2_U);
  } else {
    /* rcv_buff size is 0x100 */
    ETS_UART_INTR_ATTACH(uart0_rx_intr_handler,  &(UartDev.rcv_buff));
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, 0); // FUNC_U0RXD==0
    //PIN_PULLUP_DIS (PERIPHS_IO_MUX_U0TXD_U); now done in serbridgeInitPins
    //PIN_PULLUP_DIS (PERIPHS_IO_MUX_U0RXD_U);
  }

  uart_div_modify(uart_no, UART_CLK_FREQ / baudrate);

  if (uart_no == UART1)  //UART 1 always 8 N 1
    conf0 = CALC_UARTMODE(EIGHT_BITS, NONE_BITS, ONE_STOP_BIT);
  WRITE_PERI_REG(UART_CONF0(uart_no), conf0);

  //clear rx and tx fifo,not ready
  SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
  CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);

  if (uart_no == UART0) {
    // Configure RX interrupt conditions as follows: trigger rx-full when there are 80 characters
    // in the buffer, trigger rx-timeout when the fifo is non-empty and nothing further has been
    // received for 4 character periods.
    // Set the hardware flow-control to trigger when the FIFO holds 100 characters, although
    // we don't really expect the signals to actually be wired up to anything. It doesn't hurt
    // to set the threshold here...
    // We do not enable framing error interrupts 'cause they tend to cause an interrupt avalanche
    // and instead just poll for them when we get a std RX interrupt.
    WRITE_PERI_REG(UART_CONF1(uart_no),
                   ((80 & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) |
                   ((100 & UART_RX_FLOW_THRHD) << UART_RX_FLOW_THRHD_S) |
                   UART_RX_FLOW_EN |
                   (4 & UART_RX_TOUT_THRHD) << UART_RX_TOUT_THRHD_S |
                   UART_RX_TOUT_EN);
    SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_TOUT_INT_ENA);
  } else {
    WRITE_PERI_REG(UART_CONF1(uart_no),
                   ((UartDev.rcv_buff.TrigLvl & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S));
  }

  //clear all interrupt
  WRITE_PERI_REG(UART_INT_CLR(uart_no), 0xffff);
}


/******************************************************************************
 * FunctionName : uart1_tx_one_char
 * Description  : Internal used function
 *                Use uart1 interface to transfer one char
 * Parameters   : uint8 TxChar - character to tx
 * Returns      : OK
*******************************************************************************/
STATUS
uart_tx_one_char(uint8 uart, uint8 c)
{
  //Wait until there is room in the FIFO
  while (((READ_PERI_REG(UART_STATUS(uart))>>UART_TXFIFO_CNT_S)&UART_TXFIFO_CNT)>=100) ;
  //Send the character
  WRITE_PERI_REG(UART_FIFO(uart), c);
  return OK;
}

/******************************************************************************
 * FunctionName : uart1_write_char
 * Description  : Internal used function
 *                Do some special deal while tx char is '\r' or '\n'
 * Parameters   : char c - character to tx
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart1_write_char(char c)
{
  //if (c == '\n') uart_tx_one_char(UART1, '\r');
  uart_tx_one_char(UART1, c);
}
void ICACHE_FLASH_ATTR
uart0_write_char(char c)
{
  //if (c == '\n') uart_tx_one_char(UART0, '\r');
  uart_tx_one_char(UART0, c);
}
/******************************************************************************
 * FunctionName : uart0_tx_buffer
 * Description  : use uart0 to transfer buffer
 * Parameters   : uint8 *buf - point to send buffer
 *                uint16 len - buffer len
 * Returns      :
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart0_tx_buffer(char *buf, uint16 len)
{
  uint16 i;

  for (i = 0; i < len; i++)
  {
    uart_tx_one_char(UART0, buf[i]);
  }
}

/******************************************************************************
 * FunctionName : uart0_sendStr
 * Description  : use uart0 to transfer buffer
 * Parameters   : uint8 *buf - point to send buffer
 *                uint16 len - buffer len
 * Returns      :
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart0_sendStr(const char *str)
{
  while(*str)
  {
    uart_tx_one_char(UART0, *str++);
  }
}

static uint32 last_frm_err; // time in us when last framing error message was printed

/******************************************************************************
 * FunctionName : uart0_rx_intr_handler
 * Description  : Internal used function
 *                UART0 interrupt handler, add self handle code inside
 * Parameters   : void *para - point to ETS_UART_INTR_ATTACH's arg
 * Returns      : NONE
*******************************************************************************/
static void // must not use ICACHE_FLASH_ATTR !
uart0_rx_intr_handler(void *para)
{
  // we assume that uart1 has interrupts disabled (it uses the same interrupt vector)
  uint8 uart_no = UART0;
  const uint32 one_sec = 1000000; // one second in usecs

  // we end up largely ignoring framing errors and we just print a warning every second max
  if (READ_PERI_REG(UART_INT_RAW(uart_no)) & UART_FRM_ERR_INT_RAW) {
    uint32 now = system_get_time();
    if (last_frm_err == 0 || (now - last_frm_err) > one_sec) {
      os_printf("UART framing error (bad baud rate?)\n");
      last_frm_err = now;
    }
    // clear rx fifo (apparently this is not optional at this point)
    SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST);
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST);
    // reset framing error
    WRITE_PERI_REG(UART_INT_CLR(UART0), UART_FRM_ERR_INT_CLR);
  // once framing errors are gone for 10 secs we forget about having seen them
  } else if (last_frm_err != 0 && (system_get_time() - last_frm_err) > 10*one_sec) {
    last_frm_err = 0;
  }

  if (UART_RXFIFO_FULL_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_RXFIFO_FULL_INT_ST)
  ||  UART_RXFIFO_TOUT_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_RXFIFO_TOUT_INT_ST))
  {
    //DBG_UART("stat:%02X",*(uint8 *)UART_INT_ENA(uart_no));
    ETS_UART_INTR_DISABLE();
    post_usr_task(uart_recvTaskNum, 0);
  }
}

/******************************************************************************
 * FunctionName : uart_recvTask
 * Description  : system task triggered on receive interrupt, empties FIFO and calls callbacks
*******************************************************************************/
static void ICACHE_FLASH_ATTR
uart_recvTask(os_event_t *events)
{
  while (READ_PERI_REG(UART_STATUS(UART0)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S)) {
    //WRITE_PERI_REG(0X60000914, 0x73); //WTD // commented out by TvE

    // read a buffer-full from the uart
    uint16 length = 0;
    char buf[128];
    while ((READ_PERI_REG(UART_STATUS(UART0)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S)) &&
           (length < 128)) {
      buf[length++] = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
    }
    //DBG_UART("%d ix %d\n", system_get_time(), length);

    for (int i=0; i<MAX_CB; i++) {
      if (uart_recv_cb[i] != NULL) (uart_recv_cb[i])(buf, length);
    }
  }
  WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR|UART_RXFIFO_TOUT_INT_CLR);
  ETS_UART_INTR_ENABLE();
}

// Turn UART interrupts off and poll for nchars or until timeout hits
uint16_t ICACHE_FLASH_ATTR
uart0_rx_poll(char *buff, uint16_t nchars, uint32_t timeout_us) {
  ETS_UART_INTR_DISABLE();
  uint16_t got = 0;
  uint32_t start = system_get_time(); // time in us
  while (system_get_time()-start < timeout_us) {
    while (READ_PERI_REG(UART_STATUS(UART0)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S)) {
      buff[got++] = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
      if (got == nchars) goto done;
    }
  }
done:
  ETS_UART_INTR_ENABLE();
  return got;
}

void ICACHE_FLASH_ATTR
uart0_baud(int rate) {
  os_printf("UART %d baud\n", rate);
  uart_div_modify(UART0, UART_CLK_FREQ / rate);
}

void ICACHE_FLASH_ATTR
uart0_config(uint8_t data_bits, uint8_t parity, uint8_t stop_bits) {
  uint32_t conf0 = CALC_UARTMODE(data_bits, parity, stop_bits);
  WRITE_PERI_REG(UART_CONF0(0), conf0);
}

/******************************************************************************
 * FunctionName : uart_init
 * Description  : user interface for init uart
 * Parameters   : UartBautRate uart0_br - uart0 bautrate
 *                UartBautRate uart1_br - uart1 bautrate
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart_init(uint32 conf0, UartBautRate uart0_br, UartBautRate uart1_br)
{
  // rom use 74880 baut_rate, here reinitialize
  uart_config(UART0, uart0_br, conf0);
  uart_config(UART1, uart1_br, conf0);
  for (int i=0; i<4; i++) uart_tx_one_char(UART1, '\n');
  for (int i=0; i<4; i++) uart_tx_one_char(UART0, '\n');
  ETS_UART_INTR_ENABLE();

  // install uart1 putc callback
  os_install_putc1((void *)uart0_write_char);

  uart_recvTaskNum = register_usr_task(uart_recvTask);
}

void ICACHE_FLASH_ATTR
uart_add_recv_cb(UartRecv_cb cb) {
  for (int i=0; i<MAX_CB; i++) {
    if (uart_recv_cb[i] == NULL) {
      uart_recv_cb[i] = cb;
      return;
    }
  }
  os_printf("UART: max cb count exceeded\n");
}
