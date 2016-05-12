#include "web-server.h"

#include <espconn.h>

#include "espfs.h"
#include "config.h"
#include "cgi.h"
#include "cmd.h"
#include "serbridge.h"

#define WEB_CB "webCb"

static char* web_server_reasons[] = {
  "load", "refresh", "button", "submit"
};

char * webServerPages = NULL;

char * ICACHE_FLASH_ATTR WEB_UserPages()
{
	return webServerPages;
}

void ICACHE_FLASH_ATTR WEB_BrowseFiles()
{
	char buffer[1024];
	buffer[0] = 0;
	
	if( espFsIsValid( userPageCtx ) )
	{
		EspFsIterator it;
		espFsIteratorInit(userPageCtx, &it);
		while( espFsIteratorNext(&it) )
		{
			int nlen = os_strlen(it.name);
			if( nlen >= 6 )
			{
				if( os_strcmp( it.name + nlen-5, ".html" ) == 0 )
				{
					char sh_name[17];
					
					int spos = nlen-5;
					
					while( spos > 0 )
					{
						if( it.name[spos+1] == '/' )
							break;
						spos--;
					}
					
					int ps = nlen-5-spos;
					if( ps > 16 )
						ps = 16;
					os_memcpy(sh_name, it.name + spos, ps);
					sh_name[ps] = 0;
					
					os_strcat(buffer, ", \"");
					os_strcat(buffer, sh_name);
					os_strcat(buffer, "\", \"/");
					os_strcat(buffer, it.name);
					os_strcat(buffer, "\"");
				}
			}
			if( os_strlen(buffer) > 600 )
				break;
		}
	}
	
	if( webServerPages != NULL )
		os_free( webServerPages );
	
	int len = os_strlen(buffer) + 1;
	webServerPages = (char *)os_malloc( len );
	os_memcpy( webServerPages, buffer, len );
}

void ICACHE_FLASH_ATTR WEB_Init()
{
	espFsInit(userPageCtx, (void *)getUserPageSectionStart(), ESPFS_FLASH);
	if( espFsIsValid( userPageCtx ) )
		os_printf("Valid user file system found!\n");
	else
		os_printf("No user file system found!\n");
	WEB_BrowseFiles();
}

int ICACHE_FLASH_ATTR WEB_addJsonString(char * str, char * buf, int maxLen)
{
	char * start = buf;
	if( maxLen < 10 )
		return -1;
	char * endp = start + maxLen - 10;
	
	*buf++ = '"';
	
	int len = os_strlen(str);
	char avalbuf[len*2+1];
	char * valbuf = avalbuf;
	valbuf[len*2] = 0;
	
	httpdUrlDecode(str, len, valbuf, len * 2);
	
	while(*valbuf)
	{
		if( *valbuf == '"' || *valbuf == '\\' )
			*buf++ = '\\';
		*buf++ = *(valbuf++);
		
		if( buf > endp )
			  return -1;
	}
	
	*buf++ = '"';
	return buf - start;
}

int ICACHE_FLASH_ATTR WEB_CgiJsonHook(HttpdConnData *connData)
{
	if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
	
	void * cgiData = connData->cgiData;
	
	if( cgiData == NULL )
	{
		if( !flashConfig.slip_enable )
		{
			errorResponse(connData, 400, "Slip processing is disabled!");
			return HTTPD_CGI_DONE;
		}
		CmdCallback* cb = cmdGetCbByName( WEB_CB );
		if( cb == NULL )
		{
			errorResponse(connData, 500, "No MCU callback is registered!");
			return HTTPD_CGI_DONE;
		}
		if( serbridgeInProgramming() )
		{
			errorResponse(connData, 500, "Slip disabled at programming mode!");
			return HTTPD_CGI_DONE;
		}
		
		char reasonBuf[16];
		int i;
		int len = httpdFindArg(connData->getArgs, "reason", reasonBuf, sizeof(reasonBuf));
		if( len < 0 )
		{
			errorResponse(connData, 400, "No reason specified!");
			return HTTPD_CGI_DONE;
		}
		
		RequestReason reason = INVALID;
		for(i=0; i < sizeof(web_server_reasons)/sizeof(char *); i++)
		{
			if( os_strcmp( web_server_reasons[i], reasonBuf ) == 0 )
				reason = (RequestReason)i;
		}
		
		if( reason == INVALID )
		{
			errorResponse(connData, 400, "Invalid reason!");
			return HTTPD_CGI_DONE;
		}
		
		char body[1024];
		int  bodyLen = -1;
		
		switch(reason)
		{
			case BUTTON:
				bodyLen = httpdFindArg(connData->getArgs, "id", body, sizeof(body));
				if( bodyLen <= 0 )
				{
					errorResponse(connData, 400, "No button ID specified!");
					return HTTPD_CGI_DONE;
				}
				break;
			case SUBMIT:
				{
					if( connData->post->received < connData->post->len )
					{
						errorResponse(connData, 400, "Post too large!");
						return HTTPD_CGI_DONE;
					}
					
					bodyLen = 0;
					
					int bptr = 0;
					
					body[bodyLen++] = '{';
					
					while( bptr < connData->post->len )
					{
						char * line = connData->post->buff + bptr;
						
						char * eo = os_strchr(line, '&' );
						if( eo != NULL )
						{
							*eo = 0;
							bptr = eo - connData->post->buff + 1;
						}
						else
						{
							eo = line + os_strlen( line );
							bptr = connData->post->len;
						}
						
						int len = os_strlen(line);
						while( len >= 1 && ( line[len-1] == '\r' || line[len-1] == '\n' ))
							len--;
						line[len] = 0;
						
						char * val = os_strchr(line, '=');
						if( val != NULL )
						{
							*val = 0;
							char * name = line;
							char * value = val+1;
							
							int alen = WEB_addJsonString(name, body + bodyLen, sizeof(body) - bodyLen);
							if( alen == -1 )
							{
								errorResponse(connData, 400, "Post too large!");
								return HTTPD_CGI_DONE;
							}
							bodyLen += alen;
							body[bodyLen++] = ':';
							alen = WEB_addJsonString(value, body + bodyLen, sizeof(body) - bodyLen);
							if( alen == -1 )
							{
								errorResponse(connData, 400, "Post too large!");
								return HTTPD_CGI_DONE;
							}
							bodyLen += alen;
							body[bodyLen++] = ',';
						}
					}
					
					body[bodyLen++] = '}';
				}
				break;
			case LOAD:
			case REFRESH:
			default:
				break;
		}
		
		os_printf("Web callback to MCU: %s\n", reasonBuf);
		
		cmdResponseStart(CMD_WEB_REQ_CB, (uint32_t)cb->callback, bodyLen >= 0 ? 5 : 4);
		uint16_t r = (uint16_t)reason;
		cmdResponseBody(&r, sizeof(uint16_t));
		cmdResponseBody(&connData->conn->proto.tcp->remote_ip, 4);
		cmdResponseBody(&connData->conn->proto.tcp->remote_port, sizeof(uint16_t));
		cmdResponseBody(connData->url, os_strlen(connData->url));
		if( bodyLen >= 0 )
			cmdResponseBody(body, bodyLen);
		cmdResponseEnd();
	
		if( reason == SUBMIT )
		{
			httpdStartResponse(connData, 204);
			httpdEndHeaders(connData);
			return HTTPD_CGI_DONE;
		}
		
		connData->cgiData = (void *)1;
	}
	
	if( connData->cgiArg != NULL ) // arrived data from MCU
	{
		noCacheHeaders(connData, 200);
		httpdHeader(connData, "Content-Type", "application/json");
		char cl[16];
		os_sprintf(cl, "%d", os_strlen(connData->cgiArg));
		httpdHeader(connData, "Content-Length", cl);
		httpdEndHeaders(connData);
		httpdSend(connData, connData->cgiArg, os_strlen(connData->cgiArg));
		return HTTPD_CGI_DONE;
	}
	
	return HTTPD_CGI_MORE;
}

void ICACHE_FLASH_ATTR WEB_JsonData(CmdPacket *cmd)
{
	CmdRequest req;
	cmdRequest(&req, cmd);
	
	if (cmdGetArgc(&req) != 3) return;
	
	uint8_t ip[4];
	cmdPopArg(&req, ip, 4);
	
	uint16_t port;
	cmdPopArg(&req, &port, 2);
	
	int16_t len = cmdArgLen(&req);
	uint8_t json[len+1];
	json[len] = 0;
	cmdPopArg(&req, json, len);
	
	httpdNotify(ip, port, json);
}
