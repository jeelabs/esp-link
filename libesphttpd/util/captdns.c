#include <esp8266.h>

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


/*
This is a 'captive portal' DNS server: it basically replies with a fixed IP (in this case:
the one of the SoftAP interface of this ESP module) for any and all DNS queries. This can 
be used to send mobile phones, tablets etc which connect to the ESP in AP mode directly to
the internal webserver.
*/


typedef struct __attribute__ ((packed)) {
	uint16_t id;
	uint8_t flags;
	uint8_t rcode;
	uint16_t qdcount;
	uint16_t ancount;
	uint16_t nscount;
	uint16_t arcount;
} DnsHeader;


typedef struct __attribute__ ((packed)) {
	uint8_t len;
	uint8_t data;
} DnsLabel;


typedef struct __attribute__ ((packed)) {
	//before: label
	uint16_t type;
	uint16_t class;
} DnsQuestionFooter;


typedef struct __attribute__ ((packed)) {
	//before: label
	uint16_t type;
	uint16_t class;
	uint32_t ttl;
	uint16_t rdlength;
	//after: rdata
} DnsResourceFooter;

typedef struct __attribute__ ((packed)) {
	uint16_t prio;
	uint16_t weight;
} DnsUriHdr;


#define FLAG_QR (1<<7)
#define FLAG_AA (1<<2)
#define FLAG_TC (1<<1)
#define FLAG_RD (1<<0)

#define QTYPE_A  1
#define QTYPE_NS 2
#define QTYPE_CNAME 5
#define QTYPE_SOA 6
#define QTYPE_WKS 11
#define QTYPE_PTR 12
#define QTYPE_HINFO 13
#define QTYPE_MINFO 14
#define QTYPE_MX 15
#define QTYPE_TXT 16
#define QTYPE_URI 256

#define QCLASS_IN 1
#define QCLASS_ANY 255
#define QCLASS_URI 256


//Function to put unaligned 16-bit network values
static void ICACHE_FLASH_ATTR setn16(void *pp, int16_t n) {
	char *p=pp;
	*p++=(n>>8);
	*p++=(n&0xff);
}

//Function to put unaligned 32-bit network values
static void ICACHE_FLASH_ATTR setn32(void *pp, int32_t n) {
	char *p=pp;
	*p++=(n>>24)&0xff;
	*p++=(n>>16)&0xff;
	*p++=(n>>8)&0xff;
	*p++=(n&0xff);
}

static uint16_t ICACHE_FLASH_ATTR ntohs(uint16_t *in) {
	char *p=(char*)in;
	return ((p[0]<<8)&0xff00)|(p[1]&0xff);
}


//Parses a label into a C-string containing a dotted 
//Returns pointer to start of next fields in packet
static char* ICACHE_FLASH_ATTR labelToStr(char *packet, char *labelPtr, int packetSz, char *res, int resMaxLen) {
	int i, j, k;
	char *endPtr=NULL;
	i=0;
	do {
		if ((*labelPtr&0xC0)==0) {
			j=*labelPtr++; //skip past length
			//Add separator period if there already is data in res
			if (i<resMaxLen && i!=0) res[i++]='.';
			//Copy label to res
			for (k=0; k<j; k++) {
				if ((labelPtr-packet)>packetSz) return NULL;
				if (i<resMaxLen) res[i++]=*labelPtr++;
			}
		} else if ((*labelPtr&0xC0)==0xC0) {
			//Compressed label pointer
			endPtr=labelPtr+2;
			int offset=ntohs(((uint16_t *)labelPtr))&0x3FFF;
			//Check if offset points to somewhere outside of the packet
			if (offset>packetSz) return NULL;
			labelPtr=&packet[offset];
		}
		//check for out-of-bound-ness
		if ((labelPtr-packet)>packetSz) return NULL;
	} while (*labelPtr!=0);
	res[i]=0; //zero-terminate
	if (endPtr==NULL) endPtr=labelPtr+1;
	return endPtr;
}


//Converts a dotted hostname to the weird label form dns uses.
static char ICACHE_FLASH_ATTR *strToLabel(char *str, char *label, int maxLen) {
	char *len=label; //ptr to len byte
	char *p=label+1; //ptr to next label byte to be written
	while (1) {
		if (*str=='.' || *str==0) {
			*len=((p-len)-1);	//write len of label bit
			len=p;				//pos of len for next part
			p++;				//data ptr is one past len
			if (*str==0) break;	//done
			str++;
		} else {
			*p++=*str++;	//copy byte
//			if ((p-label)>maxLen) return NULL;	//check out of bounds
		}
	}
	*len=0;
	return p; //ptr to first free byte in resp
}


//Receive a DNS packet and maybe send a response back
static void ICACHE_FLASH_ATTR captdnsRecv(void* arg, char *pusrdata, unsigned short length) {
	struct espconn *conn=(struct espconn *)arg;
	char buff[512];
	char reply[512];
	int i;
	char *rend=&reply[length];
	char *p=pusrdata;
	DnsHeader *hdr=(DnsHeader*)p;
	DnsHeader *rhdr=(DnsHeader*)&reply[0];
	p+=sizeof(DnsHeader);
//	os_printf("DNS packet: id 0x%X flags 0x%X rcode 0x%X qcnt %d ancnt %d nscount %d arcount %d len %d\n", 
//		ntohs(&hdr->id), hdr->flags, hdr->rcode, ntohs(&hdr->qdcount), ntohs(&hdr->ancount), ntohs(&hdr->nscount), ntohs(&hdr->arcount), length);
	//Some sanity checks:
	if (length>512) return; 									//Packet is longer than DNS implementation allows
	if (length<sizeof(DnsHeader)) return; 						//Packet is too short
	if (hdr->ancount || hdr->nscount || hdr->arcount) return;	//this is a reply, don't know what to do with it
	if (hdr->flags&FLAG_TC) return;								//truncated, can't use this
	//Reply is basically the request plus the needed data
	os_memcpy(reply, pusrdata, length);
	rhdr->flags|=FLAG_QR;
	for (i=0; i<ntohs(&hdr->qdcount); i++) {
		//Grab the labels in the q string
		p=labelToStr(pusrdata, p, length, buff, sizeof(buff));
		if (p==NULL) return;
		DnsQuestionFooter *qf=(DnsQuestionFooter*)p;
		p+=sizeof(DnsQuestionFooter);
		os_printf("DNS: Q (type 0x%X class 0x%X) for %s\n", ntohs(&qf->type), ntohs(&qf->class), buff);
		if (ntohs(&qf->type)==QTYPE_A) {
			//They want to know the IPv4 address of something.
			//Build the response.
			rend=strToLabel(buff, rend, sizeof(reply)-(rend-reply)); //Add the label
			if (rend==NULL) return;
			DnsResourceFooter *rf=(DnsResourceFooter *)rend;
			rend+=sizeof(DnsResourceFooter);
			setn16(&rf->type, QTYPE_A);
			setn16(&rf->class, QCLASS_IN);
			setn32(&rf->ttl, 1);
			setn16(&rf->rdlength, 4); //IPv4 addr is 4 bytes;
			//Grab the current IP of the softap interface
			struct ip_info info;
			wifi_get_ip_info(SOFTAP_IF, &info);
			*rend++=ip4_addr1(&info.ip);
			*rend++=ip4_addr2(&info.ip);
			*rend++=ip4_addr3(&info.ip);
			*rend++=ip4_addr4(&info.ip);
			setn16(&rhdr->ancount, ntohs(&rhdr->ancount)+1);
//			os_printf("Added A rec to resp. Resp len is %d\n", (rend-reply));
		} else if (ntohs(&qf->type)==QTYPE_NS) {
			//Give ns server. Basically can be whatever we want because it'll get resolved to our IP later anyway.
			rend=strToLabel(buff, rend, sizeof(reply)-(rend-reply)); //Add the label
			DnsResourceFooter *rf=(DnsResourceFooter *)rend;
			rend+=sizeof(DnsResourceFooter);
			setn16(&rf->type, QTYPE_NS);
			setn16(&rf->class, QCLASS_IN);
			setn16(&rf->ttl, 1);
			setn16(&rf->rdlength, 4);
			*rend++=2;
			*rend++='n';
			*rend++='s';
			*rend++=0;
			setn16(&rhdr->ancount, ntohs(&rhdr->ancount)+1);
//			os_printf("Added NS rec to resp. Resp len is %d\n", (rend-reply));
		} else if (ntohs(&qf->type)==QTYPE_URI) {
			//Give uri to us
			rend=strToLabel(buff, rend, sizeof(reply)-(rend-reply)); //Add the label
			DnsResourceFooter *rf=(DnsResourceFooter *)rend;
			rend+=sizeof(DnsResourceFooter);
			DnsUriHdr *uh=(DnsUriHdr *)rend;
			rend+=sizeof(DnsUriHdr);
			setn16(&rf->type, QTYPE_URI);
			setn16(&rf->class, QCLASS_URI);
			setn16(&rf->ttl, 1);
			setn16(&rf->rdlength, 4+16);
			setn16(&uh->prio, 10);
			setn16(&uh->weight, 1);
			memcpy(rend, "http://esp.local", 16);
			rend+=16;
			setn16(&rhdr->ancount, ntohs(&rhdr->ancount)+1);
//			os_printf("Added NS rec to resp. Resp len is %d\n", (rend-reply));
		}
	}
	//Send the response
	espconn_sent(conn, (uint8*)reply, rend-reply);
}

void ICACHE_FLASH_ATTR captdnsInit(void) {
	static struct espconn conn;
	static esp_udp udpconn;
	conn.type=ESPCONN_UDP;
	conn.proto.udp=&udpconn;
	conn.proto.udp->local_port = 53;
	espconn_regist_recvcb(&conn, captdnsRecv);
	espconn_create(&conn);
}
