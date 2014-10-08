/*
Connector to let httpd use the espfs filesystem to serve the files in that.
*/

#include <string.h>
#include <osapi.h>
#include "httpd.h"
#include "espfs.h"
#include "httpdespfs.h"

int ICACHE_FLASH_ATTR cgiEspFsHook(HttpdConnData *connData) {
	EspFsFile *file=connData->cgiData;
	int len;
	char buff[1024];
	
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		espFsClose(file);
		return HTTPD_CGI_DONE;
	}

	if (file==NULL) {
		//First call to this cgi. Open the file so we can read it.
		file=espFsOpen(connData->url);
		if (file==NULL) {
			return HTTPD_CGI_NOTFOUND;
		}
		connData->cgiData=file;
		httpdStartResponse(connData, 200);
		httpdHeader(connData, "Content-Type", httpdGetMimetype(connData->url));
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

