#include "driver/uart.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"

#include "espconn.h"
#include "httpd.h"
#include "io.h"
#include "espfs.h"

//Max length of head (plus POST data)
#define MAX_HEAD_LEN 1024
//Max amount of connections
#define MAX_CONN 8

//This gets set at init time.
static HttpdBuiltInUrl *builtInUrls;

//Private data for httpd thing
struct HttpdPriv {
	char head[MAX_HEAD_LEN];
	int headPos;
};

//Connection pool
static HttpdPriv connPrivData[MAX_CONN];
static HttpdConnData connData[MAX_CONN];

static struct espconn httpdConn;
static esp_tcp httpdTcp;


typedef struct {
	const char *ext;
	const char *mimetype;
} MimeMap;

static const MimeMap mimeTypes[]={
	{"htm", "text/htm"},
	{"html", "text/html"},
	{"txt", "text/plain"},
	{"jpg", "image/jpeg"},
	{"jpeg", "image/jpeg"},
	{"png", "image/png"},
	{NULL, "text/html"},
};

const char ICACHE_FLASH_ATTR *httpdGetMimetype(char *url) {
	int i=0;
	//Go find the extension
	char *ext=url+(strlen(url)-1);
	while (ext!=url && *ext!='.') ext--;
	if (*ext=='.') ext++;

	while (mimeTypes[i].ext!=NULL && os_strcmp(ext, mimeTypes[i].ext)!=0) i++;
	return mimeTypes[i].mimetype;
}


static HttpdConnData ICACHE_FLASH_ATTR *httpdFindConnData(void *arg) {
	int i;
	for (i=0; i<MAX_CONN; i++) {
		if (connData[i].conn==(struct espconn *)arg) return &connData[i];
	}
	os_printf("FindConnData: Huh? Couldn't find connection for %p\n", arg);
	return NULL; //WtF?
}


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

static const char *httpNotFoundHeader="HTTP/1.0 404 Not Found\r\nServer: esp8266-thingie/0.1\r\n\r\nNot Found.\r\n";

void ICACHE_FLASH_ATTR httpdStartResponse(HttpdConnData *conn, int code) {
	char buff[128];
	int l;
	//ToDo: Change 'OK' according to code
	l=os_sprintf(buff, "HTTP/1.0 %d OK\r\nServer: esp8266-thingie/0.1\r\n", code);
	espconn_sent(conn->conn, buff, l);
}


void ICACHE_FLASH_ATTR httpdHeader(HttpdConnData *conn, const char *field, const char *val) {
	char buff[256];
	int l;
	l=os_sprintf(buff, "%s: %s\r\n", field, val);
	espconn_sent(conn->conn, buff, l);
}

void ICACHE_FLASH_ATTR httpdEndHeaders(HttpdConnData *conn) {
	espconn_sent(conn->conn, "\r\n", 2);
}


static void ICACHE_FLASH_ATTR httpdSentCb(void *arg) {
	int r;
	HttpdConnData *conn=httpdFindConnData(arg);
	os_printf("Sent callback on conn %p\n", conn);
	if (conn==NULL) return;
	if (conn->cgi==NULL) { //Marked for destruction?
		os_printf("Conn %p is done. Closing.\n", conn->conn);
		espconn_disconnect(conn->conn);
		conn->conn=NULL;
		return;
	}

	r=conn->cgi(conn); //Execute cgi fn.
	if (r==HTTPD_CGI_DONE) {
		conn->cgi=NULL; //mark for destruction.
	}
}

static void ICACHE_FLASH_ATTR httpdSendResp(HttpdConnData *conn) {
	int i=0;
	int r;
	//See if the url is somewhere in our internal url table.
	while (builtInUrls[i].url!=NULL) {
		os_printf("%s == %s?\n", builtInUrls[i].url, conn->url);
		if (os_strcmp(builtInUrls[i].url, conn->url)==0 || builtInUrls[i].url[0]=='*') {
			os_printf("Is url index %d\n", i);
			conn->cgiData=NULL;
			conn->cgi=builtInUrls[i].cgiCb;
			conn->cgiArg=builtInUrls[i].cgiArg;
			r=conn->cgi(conn);
			if (r!=HTTPD_CGI_NOTFOUND) {
				if (r==HTTPD_CGI_DONE) conn->cgi=NULL;  //Shouldn't happen; we haven't had a chance to send the headers yet
				return;
			}
		}
		i++;
	}
	os_printf("%s not found. 404!\n", conn->url);
	//Can't find :/
	espconn_sent(conn->conn, (uint8 *)httpNotFoundHeader, os_strlen(httpNotFoundHeader));
	conn->cgi=NULL; //mark for destruction
}

static void ICACHE_FLASH_ATTR httpdParseHeader(char *h, HttpdConnData *conn) {
	os_printf("Got header %s\n", h);
	if (os_strncmp(h, "GET ", 4)==0) {
		char *e;
		conn->url=h+4;
		e=(char*)os_strstr(conn->url, " ");
		if (e==NULL) return; //wtf?
		*e=0; //terminate url part
		os_printf("URL = %s\n", conn->url);
		conn->getArgs=(char*)os_strstr(conn->url, "?");
		if (conn->getArgs!=0) {
			int x,l;
			*conn->getArgs=0;
			conn->getArgs++;
			os_printf("GET args = %s\n", conn->getArgs);
			l=os_strlen(conn->getArgs);
			for (x=0; x<l; x++) if (conn->getArgs[x]=='&') conn->getArgs[x]=0;
			//End with double-zero
			conn->getArgs[l]=0;
			conn->getArgs[l+1]=0;
		} else {
			conn->getArgs=NULL;
		}
	}
}

static void ICACHE_FLASH_ATTR httpdRecvCb(void *arg, char *data, unsigned short len) {
	int x;
	char *p, *e;
	HttpdConnData *conn=httpdFindConnData(arg);
	if (conn==NULL) return;

	if (conn->priv->headPos==-1) return; //we don't accept data anymore

	for (x=0; x<len && conn->priv->headPos!=MAX_HEAD_LEN; x++) conn->priv->head[conn->priv->headPos++]=data[x];
	conn->priv->head[conn->priv->headPos]=0;

	//Scan for /r/n/r/n
	if ((char *)os_strstr(conn->priv->head, "\r\n\r\n")!=NULL) {
		//Reset url data
		conn->url=NULL;
		//Find end of next header line
		p=conn->priv->head;
		while(p<(&conn->priv->head[conn->priv->headPos-4])) {
			e=(char *)os_strstr(p, "\r\n");
			if (e==NULL) break;
			e[0]=0;
			httpdParseHeader(p, conn);
			p=e+2;
		}
		httpdSendResp(conn);
	}
}

static void ICACHE_FLASH_ATTR httpdReconCb(void *arg, sint8 err) {
	HttpdConnData *conn=httpdFindConnData(arg);
	os_printf("ReconCb\n");
	if (conn==NULL) return;
	//Yeah... No idea what to do here. ToDo: figure something out.
}

static void ICACHE_FLASH_ATTR httpdDisconCb(void *arg) {
#if 0
	//Stupid esp sdk passes through wrong arg here, namely the one of the *listening* socket.
	HttpdConnData *conn=httpdFindConnData(arg);
	os_printf("Disconnected, conn=%p\n", conn);
	if (conn==NULL) return;
	conn->conn=NULL;
	if (conn->cgi!=NULL) conn->cgi(conn); //flush cgi data
#endif
	//Just look at all the sockets and kill slot if needed.
	int i;
	for (i=0; i<MAX_CONN; i++) {
		if (connData[i].conn!=NULL) {
			if (connData[i].conn->state==ESPCONN_NONE || connData[i].conn->state==ESPCONN_CLOSE) {
				connData[i].conn=NULL;
				if (connData[i].cgi!=NULL) connData[i].cgi(&connData[i]); //flush cgi data
				connData[i].cgi=NULL;
			}
		}
	}
}


static void ICACHE_FLASH_ATTR httpdConnectCb(void *arg) {
	struct espconn *conn=arg;
	HttpdConnData *conndata;
	int i;
	//Find empty conndata in pool
	for (i=0; i<MAX_CONN; i++) if (connData[i].conn==NULL) break;
	os_printf("Con req, conn=%p, pool slot %d\n", conn, i);
	connData[i].priv=&connPrivData[i];
	if (i==MAX_CONN) {
		os_printf("Aiee, conn pool overflow!\n");
		espconn_disconnect(conn);
		return;
	}
	connData[i].conn=conn;
	connData[i].priv->headPos=0;

	espconn_regist_recvcb(conn, httpdRecvCb);
	espconn_regist_reconcb(conn, httpdReconCb);
	espconn_regist_disconcb(conn, httpdDisconCb);
	espconn_regist_sentcb(conn, httpdSentCb);
}


void ICACHE_FLASH_ATTR httpdInit(HttpdBuiltInUrl *fixedUrls, int port) {
	int i;

	for (i=0; i<MAX_CONN; i++) {
		connData[i].conn=NULL;
	}
	httpdConn.type=ESPCONN_TCP;
	httpdConn.state=ESPCONN_NONE;
	httpdTcp.local_port=port;
	httpdConn.proto.tcp=&httpdTcp;
	builtInUrls=fixedUrls;

	os_printf("Httpd init, conn=%p\n", &httpdConn);
	espconn_regist_connectcb(&httpdConn, httpdConnectCb);
	espconn_accept(&httpdConn);
}
