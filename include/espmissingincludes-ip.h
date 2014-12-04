#ifndef ESPMISSINGINCLUDES_IP_H
#define ESPMISSINGINCLUDES_IP_H

//Missing function prototypes in include folders. Gcc will warn on these if we don't define 'em anywhere.
//MOST OF THESE ARE GUESSED! but they seem to swork and shut up the compiler.

struct station_info *wifi_softap_get_station_info();
bool wifi_softap_set_station_info(uint8_t *addr, struct ip_addr *adr);
int igmp_leavegroup(ip_addr_t *host_ip, ip_addr_t *multicast_ip);
int igmp_joingroup(ip_addr_t *host_ip, ip_addr_t *multicast_ip);
void system_station_got_ip_set(ip_addr_t * ip_addr, ip_addr_t *sn_mask, ip_addr_t *gw_addr);
bool wifi_get_ip_info(uint8 if_index, struct ip_info *info);
#endif
