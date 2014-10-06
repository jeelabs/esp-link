/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: uart.c
 *
 * Description: Two UART mode configration and interrupt handler.
 *              Check your hardware connection while use this mode.
 *
 * Modification history:
 *     2014/3/12, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "osapi.h"
#include "driver/uart.h"

#include "driver/uart_register.h"
//#include "ssc.h"
//#include "at.h"

// UartDev is defined and initialized in rom code.
extern UartDevice    UartDev;
//extern os_event_t    at_recvTaskQueue[at_recvTaskQueueLen];

LOCAL void uart0_rx_intr_handler(void *para);

/******************************************************************************
 * FunctionName : uart_config
 * Description  : Internal used function
 *                UART0 used for data TX/RX, RX buffer size is 0x100, interrupt enabled
 *                UART1 just used for debug output
 * Parameters   : uart_no, use UART0 or UART1 defined ahead
 * Returns      : NONE
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
uart_config(uint8 uart_no)
{
  if (uart_no == UART1)
  {
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);
  }
  else
  {
    /* rcv_buff size if 0x100 */
    ETS_UART_INTR_ATTACH(uart0_rx_intr_handler,  &(UartDev.rcv_buff));
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
//    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_U0RTS);
  }

  uart_div_modify(uart_no, UART_CLK_FREQ / (UartDev.baut_rate));

  WRITE_PERI_REG(UART_CONF0(uart_no), UartDev.exist_parity
                 | UartDev.parity
                 | (UartDev.stop_bits << UART_STOP_BIT_NUM_S)
                 | (UartDev.data_bits << UART_BIT_NUM_S));

  //clear rx and tx fifo,not ready
  SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
  CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);

  //set rx fifo trigger
//  WRITE_PERI_REG(UART_CONF1(uart_no),
//                 ((UartDev.rcv_buff.TrigLvl & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) |
//                 ((96 & UART_TXFIFO_EMPTY_THRHD) << UART_TXFIFO_EMPTY_THRHD_S) |
//                 UART_RX_FLOW_EN);
  if (uart_no == UART0)
  {
    //set rx fifo trigger
    WRITE_PERI_REG(UART_CONF1(uart_no),
                   ((0x01 & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) |
                   ((0x01 & UART_RX_FLOW_THRHD) << UART_RX_FLOW_THRHD_S) |
                   UART_RX_FLOW_EN);
  }
  else
  {
    WRITE_PERI_REG(UART_CONF1(uart_no),
                   ((UartDev.rcv_buff.TrigLvl & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S));
  }

  //clear all interrupt
  WRITE_PERI_REG(UART_INT_CLR(uart_no), 0xffff);
  //enable rx_interrupt
  SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA);
}

/******************************************************************************
 * FunctionName : uart1_tx_one_char
 * Description  : Internal used function
 *                Use uart1 interface to transfer one char
 * Parameters   : uint8 TxChar - character to tx
 * Returns      : OK
*******************************************************************************/
LOCAL STATUS
uart_tx_one_char(uint8 uart, uint8 TxChar)
{
    while (true)
    {
      uint32 fifo_cnt = READ_PERI_REG(UART_STATUS(uart)) & (UART_TXFIFO_CNT<<UART_TXFIFO_CNT_S);
      if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) < 126) {
        break;
      }
    }

    WRITE_PERI_REG(UART_FIFO(uart) , TxChar);
    return OK;
}

/******************************************************************************
 * FunctionName : uart1_write_char
 * Description  : Internal used function
 *                Do some special deal while tx char is '\r' or '\n'
 * Parameters   : char c - character to tx
 * Returns      : NONE
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
uart0_write_char(char c)
{
  if (c == '\n')
  {
    uart_tx_one_char(UART0, '\r');
    uart_tx_one_char(UART0, '\n');
  }
  else if (c == '\r')
  {
  }
  else
  {
    uart_tx_one_char(UART0, c);
  }
}
/******************************************************************************
 * FunctionName : uart0_tx_buffer
 * Description  : use uart0 to transfer buffer
 * Parameters   : uint8 *buf - point to send buffer
 *                uint16 len - buffer len
 * Returns      :
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart0_tx_buffer(uint8 *buf, uint16 len)
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

/******************************************************************************
 * FunctionName : uart0_rx_intr_handler
 * Description  : Internal used function
 *                UART0 interrupt handler, add self handle code inside
 * Parameters   : void *para - point to ETS_UART_INTR_ATTACH's arg
 * Returns      : NONE
*******************************************************************************/
//extern void at_recvTask(void);

LOCAL void
uart0_rx_intr_handler(void *para)
{
  /* uart0 and uart1 intr combine togther, when interrupt occur, see reg 0x3ff20020, bit2, bit0 represents
    * uart1 and uart0 respectively
    */
//  RcvMsgBuff *pRxBuff = (RcvMsgBuff *)para;
//  uint8 RcvChar;
  uint8 uart_no = UART0;//UartDev.buff_uart_no;

//  if (UART_RXFIFO_FULL_INT_ST != (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_RXFIFO_FULL_INT_ST))
//  {
//    return;
//  }
  if (UART_RXFIFO_FULL_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_RXFIFO_FULL_INT_ST))
  {
//    at_recvTask();
    WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_RXFIFO_FULL_INT_CLR);
  }

//  WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_RXFIFO_FULL_INT_CLR);

//  if (READ_PERI_REG(UART_STATUS(uart_no)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S))
//  {
//    RcvChar = READ_PERI_REG(UART_FIFO(uart_no)) & 0xFF;
//    at_recvTask();
//    *(pRxBuff->pWritePos) = RcvChar;

//    system_os_post(at_recvTaskPrio, NULL, RcvChar);

//    //insert here for get one command line from uart
//    if (RcvChar == '\r')
//    {
//      pRxBuff->BuffState = WRITE_OVER;
//    }
//
//    pRxBuff->pWritePos++;
//
//    if (pRxBuff->pWritePos == (pRxBuff->pRcvMsgBuff + RX_BUFF_SIZE))
//    {
//      // overflow ...we may need more error handle here.
//      pRxBuff->pWritePos = pRxBuff->pRcvMsgBuff ;
//    }
//  }
}

/******************************************************************************
 * FunctionName : uart_init
 * Description  : user interface for init uart
 * Parameters   : UartBautRate uart0_br - uart0 bautrate
 *                UartBautRate uart1_br - uart1 bautrate
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart_init(UartBautRate uart0_br, UartBautRate uart1_br)
{
  // rom use 74880 baut_rate, here reinitialize
  UartDev.baut_rate = uart0_br;
  uart_config(UART0);
  UartDev.baut_rate = uart1_br;
  uart_config(UART1);
  ETS_UART_INTR_ENABLE();

  // install uart0 putc callback
  os_install_putc1((void *)uart0_write_char);
}

void ICACHE_FLASH_ATTR
uart_reattach()
{
	uart_init(BIT_RATE_74880, BIT_RATE_74880);
//  ETS_UART_INTR_ATTACH(uart_rx_intr_handler_ssc,  &(UartDev.rcv_buff));
//  ETS_UART_INTR_ENABLE();
}
