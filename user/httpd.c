/*
Esp8266 http server - core routines
*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


#include "espmissingincludes.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"

#include "espconn.h"
#include "httpd.h"
#include "io.h"
#include "espfs.h"


//Max length of request head
#define MAX_HEAD_LEN 1024
//Max amount of connections
#define MAX_CONN 8
//Max post buffer len
#define MAX_POST 1024
//Max send buffer len
#define MAX_SENDBUFF_LEN 2048


//This gets set at init time.
static HttpdBuiltInUrl *builtInUrls;

//Private data for http connection
struct HttpdPriv {
	char head[MAX_HEAD_LEN];
	int headPos;
	char *sendBuff;
	int sendBuffLen;
};

//Connection pool
static HttpdPriv connPrivData[MAX_CONN];
static HttpdConnData connData[MAX_CONN];
static HttpdPostData connPostData[MAX_CONN];

//Listening connection data
static struct espconn httpdConn;
static esp_tcp httpdTcp;

//Struct to keep extension->mime data in
typedef struct {
	const char *ext;
	const char *mimetype;
} MimeMap;

//The mappings from file extensions to mime types. If you need an extra mime type,
//add it here.
static const MimeMap mimeTypes[]={
	{"htm", "text/htm"},
	{"html", "text/html"},
	{"css", "text/css"},
	{"js", "text/javascript"},
	{"txt", "text/plain"},
	{"jpg", "image/jpeg"},
	{"jpeg", "image/jpeg"},
	{"png", "image/png"},
	{NULL, "text/html"}, //default value
};

//Returns a static char* to a mime type for a given url to a file.
const char ICACHE_FLASH_ATTR *httpdGetMimetype(char *url) {
	int i=0;
	//Go find the extension
	char *ext=url+(strlen(url)-1);
	while (ext!=url && *ext!='.') ext--;
	if (*ext=='.') ext++;
	
	//ToDo: os_strcmp is case sensitive; we may want to do case-intensive matching here...
	while (mimeTypes[i].ext!=NULL && os_strcmp(ext, mimeTypes[i].ext)!=0) i++;
	return mimeTypes[i].mimetype;
}

//Looks up the connData info for a specific esp connection
static HttpdConnData ICACHE_FLASH_ATTR *httpdFindConnData(void *arg) {
	int i;
	for (i=0; i<MAX_CONN; i++) {
		if (connData[i].conn==(struct espconn *)arg) return &connData[i];
	}
	//Shouldn't happen.
	os_printf("FindConnData: Huh? Couldn't find connection for %p\n", arg);
	return NULL;
}

//Retires a connection for re-use
static void ICACHE_FLASH_ATTR httpdRetireConn(HttpdConnData *conn) {
	if (conn->post->buff!=NULL) os_free(conn->post->buff);
	conn->post->buff=NULL;
	conn->cgi=NULL;
	conn->conn=NULL;
}

//Stupid li'l helper function that returns the value of a hex char.
static int httpdHexVal(char c) {
	if (c>='0' && c<='9') return c-'0';
	if (c>='A' && c<='F') return c-'A'+10;
	if (c>='a' && c<='f') return c-'a'+10;
	return 0;
}

//Decode a percent-encoded value.
//Takes the valLen bytes stored in val, and converts it into at most retLen bytes that
//are stored in the ret buffer. Returns the actual amount of bytes used in ret. Also
//zero-terminates the ret buffer.
int httpdUrlDecode(char *val, int valLen, char *ret, int retLen) {
	int s=0, d=0;
	int esced=0, escVal=0;
	while (s<valLen && d<retLen) {
		if (esced==1)  {
			escVal=httpdHexVal(val[s])<<4;
			esced=2;
		} else if (esced==2) {
			escVal+=httpdHexVal(val[s]);
			ret[d++]=escVal;
			esced=0;
		} else if (val[s]=='%') {
			esced=1;
		} else if (val[s]=='+') {
			ret[d++]=' ';
		} else {
			ret[d++]=val[s];
		}
		s++;
	}
	if (d<retLen) ret[d]=0;
	return d;
}

//Find a specific arg in a string of get- or post-data.
//Line is the string of post/get-data, arg is the name of the value to find. The
//zero-terminated result is written in buff, with at most buffLen bytes used. The
//function returns the length of the result, or -1 if the value wasn't found. The 
//returned string will be urldecoded already.
int ICACHE_FLASH_ATTR httpdFindArg(char *line, char *arg, char *buff, int buffLen) {
	char *p, *e;
	if (line==NULL) return 0;
	p=line;
	while(p!=NULL && *p!='\n' && *p!='\r' && *p!=0) {
		os_printf("findArg: %s\n", p);
		if (os_strncmp(p, arg, os_strlen(arg))==0 && p[strlen(arg)]=='=') {
			p+=os_strlen(arg)+1; //move p to start of value
			e=(char*)os_strstr(p, "&");
			if (e==NULL) e=p+os_strlen(p);
			os_printf("findArg: val %s len %d\n", p, (e-p));
			return httpdUrlDecode(p, (e-p), buff, buffLen);
		}
		p=(char*)os_strstr(p, "&");
		if (p!=NULL) p+=1;
	}
	os_printf("Finding %s in %s: Not found :/\n", arg, line);
	return -1; //not found
}

//Get the value of a certain header in the HTTP client head
int ICACHE_FLASH_ATTR httpdGetHeader(HttpdConnData *conn, char *header, char *ret, int retLen) {
	char *p=conn->priv->head;
	p=p+strlen(p)+1; //skip GET/POST part
	p=p+strlen(p)+1; //skip HTTP part
	while (p<(conn->priv->head+conn->priv->headPos)) {
		while(*p<=32 && *p!=0) p++; //skip crap at start
		//See if this is the header
		if (os_strncmp(p, header, strlen(header))==0 && p[strlen(header)]==':') {
			//Skip 'key:' bit of header line
			p=p+strlen(header)+1;
			//Skip past spaces after the colon
			while(*p==' ') p++;
			//Copy from p to end
			while (*p!=0 && *p!='\r' && *p!='\n' && retLen>1) {
				*ret++=*p++;
				retLen--;
			}
			//Zero-terminate string
			*ret=0;
			//All done :)
			return 1;
		}
		p+=strlen(p)+1; //Skip past end of string and \0 terminator
	}
	return 0;
}

//Start the response headers.
void ICACHE_FLASH_ATTR httpdStartResponse(HttpdConnData *conn, int code) {
	char buff[128];
	int l;
	l=os_sprintf(buff, "HTTP/1.0 %d OK\r\nServer: esp8266-httpd/"HTTPDVER"\r\n", code);
	httpdSend(conn, buff, l);
}

//Send a http header.
void ICACHE_FLASH_ATTR httpdHeader(HttpdConnData *conn, const char *field, const char *val) {
	char buff[256];
	int l;

	l=os_sprintf(buff, "%s: %s\r\n", field, val);
	httpdSend(conn, buff, l);
}

//Finish the headers.
void ICACHE_FLASH_ATTR httpdEndHeaders(HttpdConnData *conn) {
	httpdSend(conn, "\r\n", -1);
}

//ToDo: sprintf->snprintf everywhere... esp doesn't have snprintf tho' :/
//Redirect to the given URL.
void ICACHE_FLASH_ATTR httpdRedirect(HttpdConnData *conn, char *newUrl) {
	char buff[1024];
	int l;
	l=os_sprintf(buff, "HTTP/1.1 302 Found\r\nLocation: %s\r\n\r\nMoved to %s\r\n", newUrl, newUrl);
	httpdSend(conn, buff, l);
}

//Use this as a cgi function to redirect one url to another.
int ICACHE_FLASH_ATTR cgiRedirect(HttpdConnData *connData) {
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}
	httpdRedirect(connData, (char*)connData->cgiArg);
	return HTTPD_CGI_DONE;
}


//Add data to the send buffer. len is the length of the data. If len is -1
//the data is seen as a C-string.
//Returns 1 for success, 0 for out-of-memory.
int ICACHE_FLASH_ATTR httpdSend(HttpdConnData *conn, const char *data, int len) {
	if (len<0) len=strlen(data);
	if (conn->priv->sendBuffLen+len>MAX_SENDBUFF_LEN) return 0;
	os_memcpy(conn->priv->sendBuff+conn->priv->sendBuffLen, data, len);
	conn->priv->sendBuffLen+=len;
	return 1;
}

//Helper function to send any data in conn->priv->sendBuff
static void ICACHE_FLASH_ATTR xmitSendBuff(HttpdConnData *conn) {
	if (conn->priv->sendBuffLen!=0) {
		espconn_sent(conn->conn, (uint8_t*)conn->priv->sendBuff, conn->priv->sendBuffLen);
		conn->priv->sendBuffLen=0;
	}
}

//Callback called when the data on a socket has been successfully
//sent.
static void ICACHE_FLASH_ATTR httpdSentCb(void *arg) {
	int r;
	HttpdConnData *conn=httpdFindConnData(arg);
	char sendBuff[MAX_SENDBUFF_LEN];

	if (conn==NULL) return;
	conn->priv->sendBuff=sendBuff;
	conn->priv->sendBuffLen=0;

	if (conn->cgi==NULL) { //Marked for destruction?
		os_printf("Conn %p is done. Closing.\n", conn->conn);
		espconn_disconnect(conn->conn);
		httpdRetireConn(conn);
		return; //No need to call xmitSendBuff.
	}

	r=conn->cgi(conn); //Execute cgi fn.
	if (r==HTTPD_CGI_DONE) {
		conn->cgi=NULL; //mark for destruction.
	}
	if (r==HTTPD_CGI_NOTFOUND || r==HTTPD_CGI_AUTHENTICATED) {
		os_printf("ERROR! CGI fn returns code %d after sending data! Bad CGI!\n", r);
		conn->cgi=NULL; //mark for destruction.
	}
	xmitSendBuff(conn);
}

static const char *httpNotFoundHeader="HTTP/1.0 404 Not Found\r\nServer: esp8266-httpd/0.1\r\nContent-Type: text/plain\r\nContent-Length: 12\r\n\r\nNot Found.\r\n";

//This is called when the headers have been received and the connection is ready to send
//the result headers and data.
//We need to find the CGI function to call, call it, and dependent on what it returns either
//find the next cgi function, wait till the cgi data is sent or close up the connection.
static void ICACHE_FLASH_ATTR httpdProcessRequest(HttpdConnData *conn) {
	int r;
	int i=0;
	if (conn->url==NULL) {
		os_printf("WtF? url = NULL\n");
		return; //Shouldn't happen
	}
	//See if we can find a CGI that's happy to handle the request.
	while (1) {
		//Look up URL in the built-in URL table.
		while (builtInUrls[i].url!=NULL) {
			int match=0;
			//See if there's a literal match
			if (os_strcmp(builtInUrls[i].url, conn->url)==0) match=1;
			//See if there's a wildcard match
			if (builtInUrls[i].url[os_strlen(builtInUrls[i].url)-1]=='*' &&
					os_strncmp(builtInUrls[i].url, conn->url, os_strlen(builtInUrls[i].url)-1)==0) match=1;
			if (match) {
				os_printf("Is url index %d\n", i);
				conn->cgiData=NULL;
				conn->cgi=builtInUrls[i].cgiCb;
				conn->cgiArg=builtInUrls[i].cgiArg;
				break;
			}
			i++;
		}
		if (builtInUrls[i].url==NULL) {
			//Drat, we're at the end of the URL table. This usually shouldn't happen. Well, just
			//generate a built-in 404 to handle this.
			os_printf("%s not found. 404!\n", conn->url);
			httpdSend(conn, httpNotFoundHeader, -1);
			xmitSendBuff(conn);
			conn->cgi=NULL; //mark for destruction
			return;
		}
		
		//Okay, we have a CGI function that matches the URL. See if it wants to handle the
		//particular URL we're supposed to handle.
		r=conn->cgi(conn);
		if (r==HTTPD_CGI_MORE) {
			//Yep, it's happy to do so and has more data to send.
			xmitSendBuff(conn);
			return;
		} else if (r==HTTPD_CGI_DONE) {
			//Yep, it's happy to do so and already is done sending data.
			xmitSendBuff(conn);
			conn->cgi=NULL; //mark conn for destruction
			return;
		} else if (r==HTTPD_CGI_NOTFOUND || r==HTTPD_CGI_AUTHENTICATED) {
			//URL doesn't want to handle the request: either the data isn't found or there's no
			//need to generate a login screen.
			i++; //look at next url the next iteration of the loop.
		}
	}
}

//Parse a line of header data and modify the connection data accordingly.
static void ICACHE_FLASH_ATTR httpdParseHeader(char *h, HttpdConnData *conn) {
	int i;
	char first_line = false;
	
	if (os_strncmp(h, "GET ", 4)==0) {
		conn->requestType = HTTPD_METHOD_GET;
		first_line = true;
	} else if (os_strncmp(h, "POST ", 5)==0) {
		conn->requestType = HTTPD_METHOD_POST;
		first_line = true;
	}

	if (first_line) {
		char *e;
		
		//Skip past the space after POST/GET
		i=0;
		while (h[i]!=' ') i++;
		conn->url=h+i+1;

		//Figure out end of url.
		e=(char*)os_strstr(conn->url, " ");
		if (e==NULL) return; //wtf?
		*e=0; //terminate url part

		os_printf("URL = %s\n", conn->url);
		//Parse out the URL part before the GET parameters.
		conn->getArgs=(char*)os_strstr(conn->url, "?");
		if (conn->getArgs!=0) {
			*conn->getArgs=0;
			conn->getArgs++;
			os_printf("GET args = %s\n", conn->getArgs);
		} else {
			conn->getArgs=NULL;
		}

	} else if (os_strncmp(h, "Content-Length:", 15)==0) {
		i=15;
		//Skip trailing spaces
		while (h[i]==' ') i++;
		//Get POST data length
		conn->post->len=atoi(h+i);

		// Allocate the buffer
		if (conn->post->len > MAX_POST) {
			// we'll stream this in in chunks
			conn->post->buffSize = MAX_POST;
		} else {
			conn->post->buffSize = conn->post->len;
		}
		os_printf("Mallocced buffer for %d + 1 bytes of post data.\n", conn->post->buffSize);
		conn->post->buff=(char*)os_malloc(conn->post->buffSize + 1);
		conn->post->buffLen=0;
	} else if (os_strncmp(h, "Content-Type: ", 14)==0) {
		if (os_strstr(h, "multipart/form-data")) {
			// It's multipart form data so let's pull out the boundary for future use
			char *b;
			if ((b = os_strstr(h, "boundary=")) != NULL) {
				conn->post->multipartBoundary = b + 7; // move the pointer 2 chars before boundary then fill them with dashes
				conn->post->multipartBoundary[0] = '-';
				conn->post->multipartBoundary[1] = '-';
				os_printf("boundary = %s\n", conn->post->multipartBoundary);
			}
		}
	}
}


//Callback called when there's data available on a socket.
static void ICACHE_FLASH_ATTR httpdRecvCb(void *arg, char *data, unsigned short len) {
	int x;
	char *p, *e;
	char sendBuff[MAX_SENDBUFF_LEN];
	HttpdConnData *conn=httpdFindConnData(arg);
	if (conn==NULL) return;
	conn->priv->sendBuff=sendBuff;
	conn->priv->sendBuffLen=0;

	//This is slightly evil/dirty: we abuse conn->post->len as a state variable for where in the http communications we are:
	//<0 (-1): Post len unknown because we're still receiving headers
	//==0: No post data
	//>0: Need to receive post data
	//ToDo: See if we can use something more elegant for this.

	for (x=0; x<len; x++) {
		if (conn->post->len<0) {
			//This byte is a header byte.
			if (conn->priv->headPos!=MAX_HEAD_LEN) conn->priv->head[conn->priv->headPos++]=data[x];
			conn->priv->head[conn->priv->headPos]=0;
			//Scan for /r/n/r/n. Receiving this indicate the headers end.
			if (data[x]=='\n' && (char *)os_strstr(conn->priv->head, "\r\n\r\n")!=NULL) {
				//Indicate we're done with the headers.
				conn->post->len=0;
				//Reset url data
				conn->url=NULL;
				//Iterate over all received headers and parse them.
				p=conn->priv->head;
				while(p<(&conn->priv->head[conn->priv->headPos-4])) {
					e=(char *)os_strstr(p, "\r\n"); //Find end of header line
					if (e==NULL) break;			//Shouldn't happen.
					e[0]=0;						//Zero-terminate header
					httpdParseHeader(p, conn);	//and parse it.
					p=e+2;						//Skip /r/n (now /0/n)
				}
				//If we don't need to receive post data, we can send the response now.
				if (conn->post->len==0) {
					httpdProcessRequest(conn);
				}
			}
		} else if (conn->post->len!=0) {
			//This byte is a POST byte.
			conn->post->buff[conn->post->buffLen++]=data[x];
			conn->post->received++;
			if (conn->post->buffLen >= conn->post->buffSize || conn->post->received == conn->post->len) {
				//Received a chunk of post data
				conn->post->buff[conn->post->buffLen]=0; //zero-terminate, in case the cgi handler knows it can use strings
				//Send the response.
				httpdProcessRequest(conn);
				conn->post->buffLen = 0;
			}
		}
	}
}

static void ICACHE_FLASH_ATTR httpdReconCb(void *arg, sint8 err) {
	HttpdConnData *conn=httpdFindConnData(arg);
	os_printf("ReconCb\n");
	if (conn==NULL) return;
	//Yeah... No idea what to do here. ToDo: figure something out.
}

static void ICACHE_FLASH_ATTR httpdDisconCb(void *arg) {
	//The esp sdk passes through wrong arg here, namely the one of the *listening* socket.
	//That is why we can't use (HttpdConnData)arg->sock here.
	//Just look at all the sockets and kill the slot if needed.
	int i;
	for (i=0; i<MAX_CONN; i++) {
		if (connData[i].conn!=NULL) {
			//Why the >=ESPCONN_CLOSE and not ==? Well, seems the stack sometimes de-allocates
			//espconns under our noses, especially when connections are interrupted. The memory
			//is then used for something else, and we can use that to capture *most* of the
			//disconnect cases.
			if (connData[i].conn->state==ESPCONN_NONE || connData[i].conn->state>=ESPCONN_CLOSE) {
				connData[i].conn=NULL;
				if (connData[i].cgi!=NULL) connData[i].cgi(&connData[i]); //flush cgi data
				httpdRetireConn(&connData[i]);
			}
		}
	}
}


static void ICACHE_FLASH_ATTR httpdConnectCb(void *arg) {
	struct espconn *conn=arg;
	int i;
	//Find empty conndata in pool
	for (i=0; i<MAX_CONN; i++) if (connData[i].conn==NULL) break;
	os_printf("Con req, conn=%p, pool slot %d\n", conn, i);
	if (i==MAX_CONN) {
		os_printf("Aiee, conn pool overflow!\n");
		espconn_disconnect(conn);
		return;
	}
	connData[i].priv=&connPrivData[i];
	connData[i].conn=conn;
	connData[i].priv->headPos=0;
	connData[i].post=&connPostData[i];
	connData[i].post->buff=NULL;
	connData[i].post->buffLen=0;
	connData[i].post->received=0;
	connData[i].post->len=-1;

	espconn_regist_recvcb(conn, httpdRecvCb);
	espconn_regist_reconcb(conn, httpdReconCb);
	espconn_regist_disconcb(conn, httpdDisconCb);
	espconn_regist_sentcb(conn, httpdSentCb);
}

//Httpd initialization routine. Call this to kick off webserver functionality.
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
