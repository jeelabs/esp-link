/*
Connector to let httpd use the espfs filesystem to serve the files in it.
*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain
 * this notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 * Modified and enhanced by Thorsten von Eicken in 2015
 * ----------------------------------------------------------------------------
 */
#include "httpdespfs.h"

#define MAX_URL_LEN 255

// The static files marked with FLAG_GZIP are compressed and will be served with GZIP compression.
// If the client does not advertise that he accepts GZIP send following warning message (telnet users for e.g.)
static const char *gzipNonSupportedMessage = "HTTP/1.0 501 Not implemented\r\nServer: esp8266-httpd/"HTTPDVER"\r\nConnection: close\r\nContent-Type: text/plain\r\nContent-Length: 52\r\n\r\nYour browser does not accept gzip-compressed data.\r\n";

//This is a catch-all cgi function. It takes the url passed to it, looks up the corresponding
//path in the filesystem and if it exists, passes the file through. This simulates what a normal
//webserver would do with static files.
int ICACHE_FLASH_ATTR 
cgiEspFsHook(HttpdConnData *connData) {
	EspFsFile *file=connData->cgiData;
	int len;
	char buff[1024];
	char acceptEncodingBuffer[64];
	int isGzip;

	//os_printf("cgiEspFsHook conn=%p conn->conn=%p file=%p\n", connData, connData->conn, file);

	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		espFsClose(file);
		return HTTPD_CGI_DONE;
	}

	if (file==NULL) {
		//First call to this cgi. Open the file so we can read it.
		file=espFsOpen(espLinkCtx, connData->url);
		if (file==NULL) {
			if( espFsIsValid(userPageCtx) )
			{
				int maxLen = strlen(connData->url) * 2 + 1;
				if( maxLen > MAX_URL_LEN )
					maxLen = MAX_URL_LEN;
				char decodedURL[maxLen];
				httpdUrlDecode(connData->url, strlen(connData->url), decodedURL, maxLen);
				file = espFsOpen(userPageCtx, decodedURL );
				if( file == NULL )
					return HTTPD_CGI_NOTFOUND;
			}
			else
				return HTTPD_CGI_NOTFOUND;
		}

		// The gzip checking code is intentionally without #ifdefs because checking
		// for FLAG_GZIP (which indicates gzip compressed file) is very easy, doesn't
		// mean additional overhead and is actually safer to be on at all times.
		// If there are no gzipped files in the image, the code bellow will not cause any harm.

		// Check if requested file was GZIP compressed
		isGzip = espFsFlags(file) & FLAG_GZIP;
		if (isGzip) {
			// Check the browser's "Accept-Encoding" header. If the client does not
			// advertise that he accepts GZIP send a warning message (telnet users for e.g.)
			httpdGetHeader(connData, "Accept-Encoding", acceptEncodingBuffer, 64);
			if (os_strstr(acceptEncodingBuffer, "gzip") == NULL) {
				//No Accept-Encoding: gzip header present
				httpdSend(connData, gzipNonSupportedMessage, -1);
				espFsClose(file);
				return HTTPD_CGI_DONE;
			}
		}

		connData->cgiData=file;
		httpdStartResponse(connData, 200);
		httpdHeader(connData, "Content-Type", httpdGetMimetype(connData->url));
		if (isGzip) {
			httpdHeader(connData, "Content-Encoding", "gzip");
		}
		httpdHeader(connData, "Cache-Control", "max-age=3600, must-revalidate");
		httpdEndHeaders(connData);
		return HTTPD_CGI_MORE;
	}

	len=espFsRead(file, buff, 1024);
	if (len>0) espconn_sent(connData->conn, (uint8 *)buff, len);
	if (len!=1024) {
		//We're done.
		espFsClose(file);
		return HTTPD_CGI_DONE;
	} else {
		//Ok, till next time.
		return HTTPD_CGI_MORE;
	}
}

#if 0
//cgiEspFsHtml is a simple HTML file that gets prefixed by head.tpl
int ICACHE_FLASH_ATTR 
cgiEspFsHtml(HttpdConnData *connData) {
  EspFsFile *file = connData->cgiData;
	char buff[2048];

	if (connData->conn==NULL) {
		// Connection aborted. Clean up.
		if (file != NULL) espFsClose(file);
		return HTTPD_CGI_DONE;
	}

	// The first time around we send the head template in one go and we open the file
	if (file == NULL) {
		int status = 200;
		// open file, return error on failure
		file = espFsOpen("/head.tpl");
		if (file == NULL) {
			os_strcpy(buff, "Header file 'head.tpl' not found\n");
			os_printf(buff);
			status = 500;
			goto error;
		}

		// read file and return it
		int len = espFsRead(file, buff, sizeof(buff));
		espFsClose(file);
		if (len == sizeof(buff)) {
			os_sprintf(buff, "Header file 'head.tpl' too large (%d>%d)!\n", len, sizeof(buff));
			os_printf(buff);
			status = 500;
			goto error;
		}

		// before returning, open the real file for next time around
		file = espFsOpen(connData->url);
		if (file == NULL) {
			os_strcpy(buff, connData->url);
			os_strcat(buff, " not found\n");
			os_printf(buff);
			status = 404;
			goto error;
		}

		connData->cgiData = file;
		httpdStartResponse(connData, status);
		httpdHeader(connData, "Content-Type", "text/html; charset=UTF-8");
		httpdEndHeaders(connData);
		httpdSend(connData, buff, len);
		printGlobalJSON(connData);
		return HTTPD_CGI_MORE;

error: // error response
		httpdStartResponse(connData, status);
		httpdHeader(connData, "Content-Type", "text/html; charset=UTF-8");
		httpdEndHeaders(connData);
		httpdSend(connData, buff, -1);
		return HTTPD_CGI_DONE;
	}

	// The second time around send actual file
	int len = espFsRead(file, buff, sizeof(buff));
	httpdSend(connData, buff, len);
	if (len == sizeof(buff)) {
		return HTTPD_CGI_MORE;
	} else {
		connData->cgiData = NULL;
		espFsClose(file);
		return HTTPD_CGI_DONE;
	}
}
#endif

#if 0
//cgiEspFsTemplate can be used as a template.

typedef struct {
	EspFsFile *file;
	void *tplArg;
	char token[64];
	int tokenPos;
} TplData;

typedef void (* TplCallback)(HttpdConnData *connData, char *token, void **arg);

int ICACHE_FLASH_ATTR cgiEspFsTemplate(HttpdConnData *connData) {
	TplData *tpd=connData->cgiData;
	int len;
	int x, sp=0;
	char *e=NULL;
	char buff[1025];

	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		((TplCallback)(connData->cgiArg))(connData, NULL, &tpd->tplArg);
		espFsClose(tpd->file);
		os_free(tpd);
		return HTTPD_CGI_DONE;
	}

	if (tpd==NULL) {
		//First call to this cgi. Open the file so we can read it.
		tpd=(TplData *)os_malloc(sizeof(TplData));
		tpd->file=espFsOpen(connData->url);
		tpd->tplArg=NULL;
		tpd->tokenPos=-1;
		if (tpd->file==NULL) {
			espFsClose(tpd->file);
			os_free(tpd);
			return HTTPD_CGI_NOTFOUND;
		}
		if (espFsFlags(tpd->file) & FLAG_GZIP) {
			os_printf("cgiEspFsTemplate: Trying to use gzip-compressed file %s as template!\n", connData->url);
			espFsClose(tpd->file);
			os_free(tpd);
			return HTTPD_CGI_NOTFOUND;
		}
		connData->cgiData=tpd;
		httpdStartResponse(connData, 200);
		httpdHeader(connData, "Content-Type", httpdGetMimetype(connData->url));
		httpdEndHeaders(connData);
		return HTTPD_CGI_MORE;
	}

	len=espFsRead(tpd->file, buff, 1024);
	if (len>0) {
		sp=0;
		e=buff;
		for (x=0; x<len; x++) {
			if (tpd->tokenPos==-1) {
				//Inside ordinary text.
				if (buff[x]=='%') {
					//Send raw data up to now
					if (sp!=0) httpdSend(connData, e, sp);
					sp=0;
					//Go collect token chars.
					tpd->tokenPos=0;
				} else {
					sp++;
				}
			} else {
				if (buff[x]=='%') {
					if (tpd->tokenPos==0) {
						//This is the second % of a %% escape string.
						//Send a single % and resume with the normal program flow.
						httpdSend(connData, "%", 1);
					} else {
						//This is an actual token.
						tpd->token[tpd->tokenPos++]=0; //zero-terminate token
						((TplCallback)(connData->cgiArg))(connData, tpd->token, &tpd->tplArg);
					}
					//Go collect normal chars again.
					e=&buff[x+1];
					tpd->tokenPos=-1;
				} else {
					if (tpd->tokenPos<(sizeof(tpd->token)-1)) tpd->token[tpd->tokenPos++]=buff[x];
				}
			}
		}
	}
	//Send remaining bit.
	if (sp!=0) httpdSend(connData, e, sp);
	if (len!=1024) {
		//We're done.
		((TplCallback)(connData->cgiArg))(connData, NULL, &tpd->tplArg);
		espFsClose(tpd->file);
		os_free(tpd);
		return HTTPD_CGI_DONE;
	} else {
		//Ok, till next time.
		return HTTPD_CGI_MORE;
	}
}
#endif

