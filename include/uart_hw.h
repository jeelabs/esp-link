//Generated at 2012-07-03 18:44:06
/*
 *  Copyright (c) 2010 - 2011 Espressif System
 *
 */

#ifndef UART_REGISTER_H_INCLUDED
#define UART_REGISTER_H_INCLUDED

#define REG_UART_BASE( i )  (0x60000000+(i)*0xf00)
//version value:32'h062000

#define UART_FIFO( i )                          (REG_UART_BASE( i ) + 0x0)
#define UART_RXFIFO_RD_BYTE 0x000000FF
#define UART_RXFIFO_RD_BYTE_S 0

#define UART_INT_RAW( i )                       (REG_UART_BASE( i ) + 0x4)
#define UART_RXFIFO_TOUT_INT_RAW (BIT(8))
#define UART_BRK_DET_INT_RAW (BIT(7))
#define UART_CTS_CHG_INT_RAW (BIT(6))
#define UART_DSR_CHG_INT_RAW (BIT(5))
#define UART_RXFIFO_OVF_INT_RAW (BIT(4))
#define UART_FRM_ERR_INT_RAW (BIT(3))
#define UART_PARITY_ERR_INT_RAW (BIT(2))
#define UART_TXFIFO_EMPTY_INT_RAW (BIT(1))
#define UART_RXFIFO_FULL_INT_RAW (BIT(0))

#define UART_INT_ST( i )                        (REG_UART_BASE( i ) + 0x8)
#define UART_RXFIFO_TOUT_INT_ST (BIT(8))
#define UART_BRK_DET_INT_ST (BIT(7))
#define UART_CTS_CHG_INT_ST (BIT(6))
#define UART_DSR_CHG_INT_ST (BIT(5))
#define UART_RXFIFO_OVF_INT_ST (BIT(4))
#define UART_FRM_ERR_INT_ST (BIT(3))
#define UART_PARITY_ERR_INT_ST (BIT(2))
#define UART_TXFIFO_EMPTY_INT_ST (BIT(1))
#define UART_RXFIFO_FULL_INT_ST (BIT(0))

#define UART_INT_ENA( i )                       (REG_UART_BASE( i ) + 0xC)
#define UART_RXFIFO_TOUT_INT_ENA (BIT(8))
#define UART_BRK_DET_INT_ENA (BIT(7))
#define UART_CTS_CHG_INT_ENA (BIT(6))
#define UART_DSR_CHG_INT_ENA (BIT(5))
#define UART_RXFIFO_OVF_INT_ENA (BIT(4))
#define UART_FRM_ERR_INT_ENA (BIT(3))
#define UART_PARITY_ERR_INT_ENA (BIT(2))
#define UART_TXFIFO_EMPTY_INT_ENA (BIT(1))
#define UART_RXFIFO_FULL_INT_ENA (BIT(0))

#define UART_INT_CLR( i )                       (REG_UART_BASE( i ) + 0x10)
#define UART_RXFIFO_TOUT_INT_CLR (BIT(8))
#define UART_BRK_DET_INT_CLR (BIT(7))
#define UART_CTS_CHG_INT_CLR (BIT(6))
#define UART_DSR_CHG_INT_CLR (BIT(5))
#define UART_RXFIFO_OVF_INT_CLR (BIT(4))
#define UART_FRM_ERR_INT_CLR (BIT(3))
#define UART_PARITY_ERR_INT_CLR (BIT(2))
#define UART_TXFIFO_EMPTY_INT_CLR (BIT(1))
#define UART_RXFIFO_FULL_INT_CLR (BIT(0))

#define UART_CLKDIV( i )                        (REG_UART_BASE( i ) + 0x14)
#define UART_CLKDIV_CNT 0x000FFFFF
#define UART_CLKDIV_S 0

#define UART_AUTOBAUD( i )                      (REG_UART_BASE( i ) + 0x18)
#define UART_GLITCH_FILT 0x000000FF
#define UART_GLITCH_FILT_S 8
#define UART_AUTOBAUD_EN (BIT(0))

#define UART_STATUS( i )                        (REG_UART_BASE( i ) + 0x1C)
#define UART_TXD (BIT(31))
#define UART_RTSN (BIT(30))
#define UART_DTRN (BIT(29))
#define UART_TXFIFO_CNT 0x000000FF
#define UART_TXFIFO_CNT_S 16
#define UART_RXD (BIT(15))
#define UART_CTSN (BIT(14))
#define UART_DSRN (BIT(13))
#define UART_RXFIFO_CNT 0x000000FF
#define UART_RXFIFO_CNT_S 0

#define UART_CONF0( i )                         (REG_UART_BASE( i ) + 0x20)
#define UART_DTR_INV (BIT(24))
#define UART_RTS_INV (BIT(23))
#define UART_TXD_INV (BIT(22))
#define UART_DSR_INV (BIT(21))
#define UART_CTS_INV (BIT(20))
#define UART_RXD_INV (BIT(19))
#define UART_INVERT_BIT_NUM 0x0000003F
#define UART_INVERT_BIT_NUM_S 19
#define UART_NO_INVERT (0)
#define UART_TXFIFO_RST (BIT(18))
#define UART_RXFIFO_RST (BIT(17))
#define UART_IRDA_EN (BIT(16))
#define UART_TX_FLOW_EN (BIT(15))
#define UART_LOOPBACK (BIT(14))
#define UART_IRDA_RX_INV (BIT(13))
#define UART_IRDA_TX_INV (BIT(12))
#define UART_IRDA_WCTL (BIT(11))
#define UART_IRDA_TX_EN (BIT(10))
#define UART_IRDA_DPLX (BIT(9))
#define UART_TXD_BRK (BIT(8))
#define UART_SW_DTR (BIT(7))
#define UART_SW_RTS (BIT(6))
#define UART_STOP_BIT_NUM 0x00000003
#define UART_STOP_BIT_NUM_S 4
#define UART_BIT_NUM 0x00000003
#define UART_BIT_NUM_S 2
#define UART_PARITY_EN (BIT(1))
#define UART_PARITY (BIT(0))

#define UART_CONF1( i )                         (REG_UART_BASE( i ) + 0x24)
#define UART_RX_TOUT_EN (BIT(31))
#define UART_RX_TOUT_THRHD 0x0000007F
#define UART_RX_TOUT_THRHD_S 24
#define UART_RX_FLOW_EN (BIT(23))
#define UART_RX_FLOW_THRHD 0x0000007F
#define UART_RX_FLOW_THRHD_S 16
#define UART_TXFIFO_EMPTY_THRHD 0x0000007F
#define UART_TXFIFO_EMPTY_THRHD_S 8
#define UART_RXFIFO_FULL_THRHD 0x0000007F
#define UART_RXFIFO_FULL_THRHD_S 0

#define UART_LOWPULSE( i )                      (REG_UART_BASE( i ) + 0x28)
#define UART_LOWPULSE_MIN_CNT         0x000FFFFF
#define UART_LOWPULSE_MIN_CNT_S     0

#define UART_HIGHPULSE( i )                     (REG_UART_BASE( i ) + 0x2C)
#define UART_HIGHPULSE_MIN_CNT         0x000FFFFF
#define UART_HIGHPULSE_MIN_CNT_S     0

#define UART_PULSE_NUM( i )                     (REG_UART_BASE( i ) + 0x30)
#define UART_PULSE_NUM_CNT                  0x0003FF
#define UART_PULSE_NUM_CNT_S               0

#define UART_DATE( i )                          (REG_UART_BASE( i ) + 0x78)
#define UART_ID( i )                            (REG_UART_BASE( i ) + 0x7C)

#define RX_BUFF_SIZE    256
#define TX_BUFF_SIZE    100
#define UART0   0
#define UART1   1

//calc bit 0..5 for  UART_CONF0 register
#define CALC_UARTMODE(data_bits,parity,stop_bits, invert_bits) \
	(((parity == NONE_BITS) ? 0x0 : (UART_PARITY_EN | (parity & UART_PARITY))) | \
	((stop_bits & UART_STOP_BIT_NUM) << UART_STOP_BIT_NUM_S) | \
	((data_bits & UART_BIT_NUM) << UART_BIT_NUM_S) | \
    ((invert_bits & UART_INVERT_BIT_NUM) << UART_INVERT_BIT_NUM_S))

typedef enum {
    FIVE_BITS = 0x0,
    SIX_BITS = 0x1,
    SEVEN_BITS = 0x2,
    EIGHT_BITS = 0x3
} UartBitsNum4Char;

typedef enum {
    ONE_STOP_BIT             = 0,
    ONE_HALF_STOP_BIT        = BIT2,
    TWO_STOP_BIT             = BIT2
} UartStopBitsNum;

typedef enum {
    NONE_BITS = 0,
    ODD_BITS   = 0,
    EVEN_BITS = BIT4
} UartParityMode;

typedef enum {
    STICK_PARITY_DIS   = 0,
    STICK_PARITY_EN    = BIT3 | BIT5
} UartExistParity;


typedef enum {
    BIT_RATE_300    = 300,
    BIT_RATE_600    = 600,
    BIT_RATE_1200   = 1200,
    BIT_RATE_2400   = 2400,
    BIT_RATE_4800   = 4800,
    BIT_RATE_9600     = 9600,
    BIT_RATE_19200   = 19200,
    BIT_RATE_38400   = 38400,
    BIT_RATE_57600   = 57600,
    BIT_RATE_74880   = 74880,
    BIT_RATE_115200 = 115200,
    BIT_RATE_230400 = 230400,
    BIT_RATE_460800 = 460800,
    BIT_RATE_921600 = 921600
} UartBautRate;

typedef enum {
    NONE_CTRL,
    HARDWARE_CTRL,
    XON_XOFF_CTRL
} UartFlowCtrl;

typedef enum {
    EMPTY,
    UNDER_WRITE,
    WRITE_OVER
} RcvMsgBuffState;

typedef struct {
    uint32     RcvBuffSize;
    uint8     *pRcvMsgBuff;
    uint8     *pWritePos;
    uint8     *pReadPos;
    uint8      TrigLvl; //JLU: may need to pad
    RcvMsgBuffState  BuffState;
} RcvMsgBuff;

typedef struct {
    uint32   TrxBuffSize;
    uint8   *pTrxBuff;
} TrxMsgBuff;

typedef enum {
    BAUD_RATE_DET,
    WAIT_SYNC_FRM,
    SRCH_MSG_HEAD,
    RCV_MSG_BODY,
    RCV_ESC_CHAR,
} RcvMsgState;

typedef struct {
    UartBautRate 	     baut_rate;
    UartBitsNum4Char  data_bits;
    UartExistParity      exist_parity;
    UartParityMode 	    parity;
    UartStopBitsNum   stop_bits;
    UartFlowCtrl         flow_ctrl;
    RcvMsgBuff          rcv_buff;
    TrxMsgBuff           trx_buff;
    RcvMsgState        rcv_state;
    int                      received;
    int                      buff_uart_no;  //indicate which uart use tx/rx buffer
} UartDevice;

#endif // UART_REGISTER_H_INCLUDED
