#include "driver/uart.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"

#include "espconn.h"
#include "httpd.h"
#include "io.h"

#define MAX_HEAD_LEN 1024

int cgiSet(struct espconn *conn);

typedef int (* cgiSendCallback)(struct espconn *conn);

typedef struct {
	const char *url;
	const char *fixedResp;
	cgiSendCallback cgiCb;
} UrlData;

const char htmlIndex[]="<html><head><title>Hello World</title></head> \
<body><h1>Hello, World!</h1></body> \
</html>";


static const UrlData urls[]={
	{"/", htmlIndex, NULL},
	{"/set", NULL, cgiSet},
	{NULL, NULL, NULL},
};

static struct espconn conn;
static esp_tcp tcp;

static int recLen;
static char recBuff[MAX_HEAD_LEN];

typedef struct {
	char *url;
	char *getArgs;
	const UrlData *effUrl;
} GetData;

static GetData getData;

//Find a specific arg in a string of get- or post-data.
static char *ICACHE_FLASH_ATTR httpdFindArg(char *line, char *arg) {
	char *p;
	int al;
	if (line==NULL) return NULL;

	p=line;
	al=os_strlen(arg);
	os_printf("Finding %s in %s\n", arg, line);

	while (p[0]!=0) {
		if (os_strncmp(p, arg, al)==0 && p[al]=='=') {
			//Gotcha.
			return &p[al+1];
		} else {
			//Wrong arg. Advance to start of next arg.
			p+=os_strlen(p)+1;
		}
	}
	os_printf("Finding %s in %s: Not found :/\n", arg, line);
	return NULL; //not found
}

//ToDo: Move cgi functions to somewhere else
int cgiSet(struct espconn *conn) {
	char *on;
	static const char okStr[]="<html><head><title>OK</title></head><body><p>OK</p></body></html>";
	on=httpdFindArg(getData.getArgs, "led");
	os_printf("cgiSet: on=%s\n", on?on:"not found");
	if (on!=NULL) ioLed(atoi(on));
	espconn_sent(conn, (uint8 *)okStr, os_strlen(okStr));
	return 1;
}




static const char *httpOkHeader="HTTP/1.0 200 OK\r\nServer: esp8266-thingie/0.1\r\nContent-Type: text/html\r\n\r\n";
static const char *httpNotFoundHeader="HTTP/1.0 404 Not Found\r\nServer: esp8266-thingie/0.1\r\n\r\nNot Found.\r\n";

static void ICACHE_FLASH_ATTR httpdSentCb(void *arg) {
	struct espconn *conn=arg;
	if (getData.effUrl==NULL) {
		espconn_disconnect(conn);
		return;
	}

	if (getData.effUrl->cgiCb!=NULL) {
		if (getData.effUrl->cgiCb(conn)) getData.effUrl=NULL;
	} else {
		espconn_sent(conn, (uint8 *)getData.effUrl->fixedResp, os_strlen(getData.effUrl->fixedResp));
		getData.effUrl=NULL;
	}
}

static void ICACHE_FLASH_ATTR httpdSendResp(struct espconn *conn) {
	int i=0;
	while (urls[i].url!=NULL) {
		if (os_strcmp(urls[i].url, getData.url)==0) {
			getData.effUrl=&urls[i];
			os_printf("Is url index %d\n", i);
			espconn_sent(conn, (uint8 *)httpOkHeader, os_strlen(httpOkHeader));
			return;
		}
		i++;
	}
	//Can't find :/
	espconn_sent(conn, (uint8 *)httpNotFoundHeader, os_strlen(httpNotFoundHeader));
}

static void ICACHE_FLASH_ATTR httpdParseHeader(char *h) {
	os_printf("Got header %s\n", h);
	if (os_strncmp(h, "GET ", 4)==0) {
		char *e;
		getData.url=h+4;
		e=(char*)os_strstr(getData.url, " ");
		if (e==NULL) return; //wtf?
		*e=0; //terminate url part
		os_printf("URL = %s\n", getData.url);
		getData.getArgs=(char*)os_strstr(getData.url, "?");
		if (getData.getArgs!=0) {
			int x,l;
			*getData.getArgs=0;
			getData.getArgs++;
			os_printf("GET args = %s\n", getData.getArgs);
			l=os_strlen(getData.getArgs);
			for (x=0; x<l; x++) if (getData.getArgs[x]=='&') getData.getArgs[x]=0;
			//End with double-zero
			getData.getArgs[l]=0;
			getData.getArgs[l+1]=0;
		} else {
			getData.getArgs=NULL;
		}
	}
}

static void ICACHE_FLASH_ATTR httpdRecvCb(void *arg, char *data, unsigned short len) {
	struct espconn *conn=arg;
	int x;
	char *p, *e;

	if (recLen==-1) return; //we don't accept data anymore
	for (x=0; x<len && recLen!=MAX_HEAD_LEN; x++) recBuff[recLen++]=data[x];
	recBuff[recLen]=0;

	//Scan for /r/n/r/n
	if ((char *)os_strstr(recBuff, "\r\n\r\n")!=NULL) {
		//Reset url data
		getData.url=NULL;
		//Find end of next header line
		p=recBuff;
		while(p<(&recBuff[recLen-4])) {
			e=(char *)os_strstr(p, "\r\n");
			if (e==NULL) break;
			e[0]=0;
			httpdParseHeader(p);
			p=e+2;
		}
		httpdSendResp(conn);
	}
}

static void ICACHE_FLASH_ATTR httpdReconCb(void *arg, sint8 err) {
	os_printf("ReconCb\n");
	httpdInit();
}

static void ICACHE_FLASH_ATTR httpdDisconCb(void *arg) {
	struct espconn *conn=arg;
	os_printf("Disconnected, conn=%p\n", conn);
}


static void ICACHE_FLASH_ATTR httpdConnectCb(void *arg) {
	struct espconn *conn=arg;
	os_printf("Con req, conn=%p\n", conn);
	recLen=0;
	espconn_regist_recvcb(conn, httpdRecvCb);
	espconn_regist_reconcb(conn, httpdReconCb);
	espconn_regist_disconcb(conn, httpdDisconCb);
	espconn_regist_sentcb(conn, httpdSentCb);
}


void ICACHE_FLASH_ATTR httpdInit() {
	conn.type=ESPCONN_TCP;
	conn.state=ESPCONN_NONE;
	tcp.local_port=80;
	conn.proto.tcp=&tcp;
	os_printf("Httpd init, conn=%p\n", conn);
	espconn_regist_connectcb(&conn, httpdConnectCb);
	espconn_accept(&conn);
}
