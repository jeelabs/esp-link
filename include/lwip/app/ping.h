#ifndef __PING_H__
#define __PING_H__
#include "lwip/ip_addr.h"
#include "lwip/icmp.h"
/**
 * PING_USE_SOCKETS: Set to 1 to use sockets, otherwise the raw api is used
 */
#ifndef PING_USE_SOCKETS
#define PING_USE_SOCKETS    LWIP_SOCKET
#endif


void ping_init(void)ICACHE_FLASH_ATTR;
void inline set_ping_length(u16_t ping_length)ICACHE_FLASH_ATTR;
u16_t inline get_ping_length()ICACHE_FLASH_ATTR;

#if !PING_USE_SOCKETS
void ping_send_now(void)ICACHE_FLASH_ATTR;
#endif /* !PING_USE_SOCKETS */

#ifdef SSC
void ping_set_target(ip_addr_t *ip)ICACHE_FLASH_ATTR;
void ping_set_recv_cb(void (*ping_recv_cb)(struct pbuf *p, struct icmp_echo_hdr *iecho))ICACHE_FLASH_ATTR;
#endif /* SSC */

#endif /* __PING_H__ */
