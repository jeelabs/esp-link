#ifndef __UART_H__
#define __UART_H__

#include "uart_hw.h"

// Receive callback function signature
typedef void (*UartRecv_cb)(char *buf, int len);

// Initialize UARTs to the provided baud rates (115200 recommended). This also makes the os_printf
// calls use uart1 for output (for debugging purposes)
void ICACHE_FLASH_ATTR uart_init(UartBautRate uart0_br, UartBautRate uart1_br);

// Transmit a buffer of characters on UART0
void ICACHE_FLASH_ATTR uart0_tx_buffer(char *buf, uint16 len);

void ICACHE_FLASH_ATTR uart0_write_char(char c);
STATUS uart_tx_one_char(uint8 uart, uint8 c);

// Add a receive callback function, this is called on the uart receive task each time a chunk
// of bytes are received. A small number of callbacks can be added and they are all called
// with all new characters.
void ICACHE_FLASH_ATTR uart_add_recv_cb(UartRecv_cb cb);

void ICACHE_FLASH_ATTR uart0_baud(int rate);

#endif /* __UART_H__ */
