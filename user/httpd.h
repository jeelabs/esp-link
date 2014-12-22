#ifndef HTTPD_H
#define HTTPD_H
#include <c_types.h>
#include <ip_addr.h>
#include <espconn.h>

#define HTTPDVER "0.2"

#define HTTPD_CGI_MORE 0
#define HTTPD_CGI_DONE 1
#define HTTPD_CGI_NOTFOUND 2
#define HTTPD_CGI_AUTHENTICATED 2 //for now

typedef struct HttpdPriv HttpdPriv;
typedef struct HttpdConnData HttpdConnData;

typedef int (* cgiSendCallback)(HttpdConnData *connData);

//A struct describing a http connection. This gets passed to cgi functions.
struct HttpdConnData {
	struct espconn *conn;
	char *url;
	char *getArgs;
	const void *cgiArg;
	void *cgiData;
	HttpdPriv *priv;
	cgiSendCallback cgi;
	int postLen;
	char *postBuff;
};

//A struct describing an url. This is the main struct that's used to send different URL requests to
//different routines.
typedef struct {
	const char *url;
	cgiSendCallback cgiCb;
	const void *cgiArg;
} HttpdBuiltInUrl;

int ICACHE_FLASH_ATTR cgiRedirect(HttpdConnData *connData);
void ICACHE_FLASH_ATTR httpdRedirect(HttpdConnData *conn, char *newUrl);
int httpdUrlDecode(char *val, int valLen, char *ret, int retLen);
int ICACHE_FLASH_ATTR httpdFindArg(char *line, char *arg, char *buff, int buffLen);
void ICACHE_FLASH_ATTR httpdInit(HttpdBuiltInUrl *fixedUrls, int port);
const char *httpdGetMimetype(char *url);
void ICACHE_FLASH_ATTR httpdStartResponse(HttpdConnData *conn, int code);
void ICACHE_FLASH_ATTR httpdHeader(HttpdConnData *conn, const char *field, const char *val);
void ICACHE_FLASH_ATTR httpdEndHeaders(HttpdConnData *conn);
int ICACHE_FLASH_ATTR httpdGetHeader(HttpdConnData *conn, char *header, char *ret, int retLen);
int ICACHE_FLASH_ATTR httpdSend(HttpdConnData *conn, const char *data, int len);

#endif