/*
* mdns.h
*
*  Simple mDNS responder.
*  It replys to mDNS IP (IPv4/A) queries and optionally broadcasts ip advertisements
*  using the mDNS protocol
*  Created on: Apr 10, 2015
*      Author: Kevin Uhlir (n0bel)
*
*/

#include <esp8266.h>
#include "mdns.h"

#ifdef MDNS_DBG
#define DBG(format, ...) os_printf(format, ## __VA_ARGS__)
#else
#define DBG(format, ...) do { } while(0)
#endif

static int mdns_initialized = 0;
static int ttl = 0;
static os_timer_t bc_timer;

static struct espconn mdns_conn;

#define MAX_HOSTS 5
static struct _host {
  char *hostname;
  unsigned char *mdns;
  struct ip_addr ip;
  int len;
} hosts[MAX_HOSTS] = { { NULL } };

int nhosts = 0;

struct ip_addr mdns_multicast;
struct ip_addr ipaddr_any;

#ifdef MDNS_DBG

void ICACHE_FLASH_ATTR hexdump(char *desc, void *addr, int len) {
  int i;
  unsigned char buff[17];
  unsigned char *pc = (unsigned char*)addr;

  // Output description if given.
  if (desc != NULL)
    DBG("%s:\n", desc);

  // Process every byte in the data.
  for (i = 0; i < len; i++) {
    // Multiple of 16 means new line (with line offset).

    if ((i % 16) == 0) {
      // Just don't print ASCII for the zeroth line.
      if (i != 0)
        DBG("  %s\n", buff);

      // Output the offset.
      DBG("  %04x ", i);
    }

    // Now the hex code for the specific character.
    DBG(" %02x", pc[i]);

    // And store a printable ASCII character for later.
    if ((pc[i] < 0x20) || (pc[i] > 0x7e))
      buff[i % 16] = '.';
    else
      buff[i % 16] = pc[i];
    buff[(i % 16) + 1] = '\0';
  }

  // Pad out last line if not exactly 16 characters.
  while ((i % 16) != 0) {
    DBG("   ");
    i++;
  }

  // And print the final ASCII bit.
  DBG("  %s\n", buff);
}
#endif

unsigned ICACHE_FLASH_ATTR char * encodeResp(struct ip_addr * addr, char *name, int *len)
{
  *len = 12 + strlen(name) + 16;
  unsigned char *data = (unsigned char *)os_zalloc(*len);

  data[2] = 0x84;
  data[7] = 1;

  unsigned char *p = data + 12;
  char *np = name;
  char *i;
  while ((i = strchr(np, '.')))
  {
    *p = i - np;
    p++;
    memcpy(p, np, i - np);
    p += i - np;
    np = i + 1;
  }
  *p = strlen(np);
  p++;
  memcpy(p, np, strlen(np));
  p += strlen(np);
  *p++ = 0; // terminate string sequence

  *p++ = 0; *p++ = 1; // type 0001 (A)

  *p++ = 0x80; *p++ = 1; // class code (IPV4)

  *p++ = ttl >> 24;
  *p++ = ttl >> 16;
  *p++ = ttl >> 8;
  *p++ = ttl;

  *p++ = 0; *p++ = 4; // length (of ip)

  memcpy(p, addr, 4); 
  return data;
}

char ICACHE_FLASH_ATTR *decode_name_strings(unsigned char *p, int *len)
{
  unsigned char *s = p;
  int c;
  *len = 0;
  while ((c = *s++))
  {
    *len += c + 1;
    s += c;
  }
  char *name = (char *)os_zalloc(*len + 1);
  char *np = name;
  s = p;
  while ((c = *s++))
  {
    memcpy(np, s, c);
    np += c;
    *np++ = '.';
    s += c;
  }
  np--; *np = 0;
  return name;
}

void ICACHE_FLASH_ATTR sendOne(struct _host *h)
{
  mdns_conn.proto.udp->remote_ip[0] = 224;
  mdns_conn.proto.udp->remote_ip[1] = 0;
  mdns_conn.proto.udp->remote_ip[2] = 0;
  mdns_conn.proto.udp->remote_ip[3] = 251;
  mdns_conn.proto.udp->remote_port = 5353;
  mdns_conn.proto.udp->local_port = 5353;
#ifdef MDNS_DBG
//  hexdump("sending", h->mdns, h->len);
#endif
  espconn_sent(&mdns_conn, h->mdns, h->len);

}
void ICACHE_FLASH_ATTR decodeQuery(unsigned char *data)
{
  if (data[0] != 0 || data[1] != 0 || data[2] != 0 || data[3] != 0) return;  // only queries

  int qcount = data[5]; // purposely ignore qcount > 255
  unsigned char *p = data + 12;
  char *name;
  int len;
  while (qcount-- > 0)
  {
    if (*p == 0xc0) // pointer
    {
      name = decode_name_strings(data + p[1], &len);
      p += 2;
    }
    else
    {
      name = decode_name_strings(p, &len);
      p += len + 1;
    }
    int qtype = p[0] * 256 + p[1];
    int qclass = p[2] * 256 + p[3];
    p += 4;
//    DBG("decoded name  %s qtype=%d qclass=%d\n", name, qtype, qclass);

    if (qtype == 1 && (qclass & 0x7fff) == 1)
    {
      struct _host *h;
      int i;
      for (h = hosts, i = 0; i < nhosts; i++, h++)
      {
        if (h->hostname && strcmp(name, h->hostname) == 0)
        {
//          DBG("its %s!\n", h->hostname);
          sendOne(h);
        }
      }
      os_free(name);
    }
  }

  return;
}

static void ICACHE_FLASH_ATTR broadcast(void *arg)
{
  struct _host *h;
  int i;
  for (h = hosts, i = 0; i < nhosts; i++, h++)
  {
//    DBG("broadcast for %s\n", h->hostname);
    sendOne(h);
  }
}

static void ICACHE_FLASH_ATTR mdns_udp_recv(void *arg, char *pusrdata, unsigned short length) {
  unsigned char *data = (unsigned char *)pusrdata;


//  DBG("remote port=%d remoteip=%d.%d.%d.%d\n", mdns_conn.proto.udp->remote_port,
//    mdns_conn.proto.udp->remote_ip[0],
//    mdns_conn.proto.udp->remote_ip[1],
//    mdns_conn.proto.udp->remote_ip[2],
//    mdns_conn.proto.udp->remote_ip[3]
//    );
#ifdef MDNS_DBG
//  hexdump("rec mNDS:", data, length);
#endif
  decodeQuery(data);
}

int ICACHE_FLASH_ATTR mdns_addhost(char *hn, struct ip_addr* ip)
{
  struct _host *h = &hosts[nhosts];
  if (nhosts >= MAX_HOSTS)
  {
    int i;
    for (h = hosts, i = 0; i < MAX_HOSTS; i++, h++)
    {
      if (!h->hostname) break;
    }
    if (i >= MAX_HOSTS) return -1;
  }
  else
  {
    nhosts++;
  }
  h->hostname = (char *)os_zalloc(os_strlen(hn) + 1);
  os_strcpy(h->hostname, hn);
  h->ip.addr = ip->addr;
  h->mdns = encodeResp(ip, hn, &(h->len));
//  DBG("resp len: %d\n", ets_strlen(h->mdns));
#ifdef MDNS_DBG
  hexdump("addhost", hosts, sizeof(hosts));
#endif
  return(0);
}

int ICACHE_FLASH_ATTR mdns_delhost(char *hn, struct ip_addr* ip)
{
  if (nhosts < 1) return -1;
  struct _host *h;
  int i;
  for (h = hosts, i = 0; i < MAX_HOSTS; i++, h++)
  {
    if ((hn && strcmp(hn, h->hostname) == 0)
      || (ip && h->ip.addr == ip->addr))
    {
      if (h->hostname) os_free(h->hostname);
      if (h->mdns) os_free(h->mdns);
      h->ip.addr = 0;
    }
  }
#ifdef MDNS_DBG
//  hexdump("delhost", hosts, sizeof(hosts));
#endif
  return(0);
}

int ICACHE_FLASH_ATTR mdns_init(int bctime, int sttl)
{
  if (mdns_initialized) mdns_stop();
  mdns_initialized = 1;
  ttl = sttl;

  IP4_ADDR(&mdns_multicast, 224, 0, 0, 251);
  ipaddr_any.addr = IPADDR_ANY;

  espconn_igmp_join(&ipaddr_any, &mdns_multicast);

  static esp_udp mdns_udp;
  mdns_conn.type = ESPCONN_UDP;
  mdns_conn.state = ESPCONN_NONE;
  mdns_conn.proto.udp = &mdns_udp;
  mdns_udp.local_port = 5353;
  mdns_conn.reverse = NULL;

  espconn_regist_recvcb(&mdns_conn, mdns_udp_recv);
  espconn_create(&mdns_conn);

  os_timer_disarm(&bc_timer);
  if (bctime > 0)
  {
    os_timer_setfn(&bc_timer, (os_timer_func_t *)broadcast, (void *)0);
    os_timer_arm(&bc_timer, bctime * 1000, 1);
  }

  return 0;
}

int ICACHE_FLASH_ATTR mdns_stop()
{
  if (!mdns_initialized) return(0);
  os_timer_disarm(&bc_timer);
  espconn_disconnect(&mdns_conn);
  espconn_delete(&mdns_conn);
  espconn_igmp_leave(&ipaddr_any, &mdns_multicast);
  mdns_initialized = 0;
  struct _host *h;
  int i;
  for (h = hosts, i = 0; i < MAX_HOSTS; i++, h++)
  {
    if (h->hostname) os_free(h->hostname);
    if (h->mdns) os_free(h->mdns);
    h->ip.addr = 0;
  }
  nhosts = 0;
  return(0);
}