#include "web-server.h"

#include <espconn.h>

#include "espfs.h"
#include "config.h"
#include "cgi.h"
#include "cmd.h"
#include "serbridge.h"

#define WEB_CB "webCb"

#define MAX_VARS 20

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
		
		char  body[1024];
		int   bodyPtr = 0;
		int   argNum = 0;
		char *argPos[MAX_VARS];
		int   argLen[MAX_VARS];
		
		switch(reason)
		{
			case BUTTON:
				argLen[0] = httpdFindArg(connData->getArgs, "id", body, sizeof(body));
				if( argLen[0] <= 0 )
				{
					errorResponse(connData, 400, "No button ID specified!");
					return HTTPD_CGI_DONE;
				}
				argPos[0] = body;
				argNum++;
				break;
			case SUBMIT:
				{
					if( connData->post->received < connData->post->len )
					{
						errorResponse(connData, 400, "Post too large!");
						return HTTPD_CGI_DONE;
					}
					
					int bptr = 0;
					
					while( bptr < connData->post->len )
					{
						if( argNum >= MAX_VARS )
						{
							errorResponse(connData, 400, "Too many variables!");
							return HTTPD_CGI_DONE;
						}
						
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
							
							int namLen = os_strlen(name);
							int valLen = os_strlen(value);
							
							int totallen = namLen + valLen + 2;
							if( bodyPtr + totallen > sizeof(body) - 10 )
							{
								errorResponse(connData, 400, "Post too large!");
								return HTTPD_CGI_DONE;
							}
							
							argPos[argNum] = body + bodyPtr;
							
							body[bodyPtr++] = (char)WEB_STRING;
							os_strcpy( body + bodyPtr, name );
							bodyPtr += namLen;
							body[bodyPtr++] = 0;
							
							os_strcpy( body + bodyPtr, value );
							bodyPtr += valLen;
							
							argLen[argNum++] = totallen;
						}
					}
				}
				break;
			case LOAD:
			case REFRESH:
			default:
				break;
		}
		
		os_printf("Web callback to MCU: %s\n", reasonBuf);
		
		cmdResponseStart(CMD_WEB_REQ_CB, (uint32_t)cb->callback, 4 + argNum);
		uint16_t r = (uint16_t)reason;
		cmdResponseBody(&r, sizeof(uint16_t));
		cmdResponseBody(&connData->conn->proto.tcp->remote_ip, 4);
		cmdResponseBody(&connData->conn->proto.tcp->remote_port, sizeof(uint16_t));
		cmdResponseBody(connData->url, os_strlen(connData->url));
		
		int j;
		for( j=0; j < argNum; j++ )
			cmdResponseBody(argPos[j], argLen[j]);
		
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
		char jsonBuf[1500];
		int  jsonPtr = 0;
		
		
		jsonBuf[jsonPtr++] = '{';
		CmdRequest * req = (CmdRequest *)(connData->cgiArg);
		
		int c = 2;
		while( c++ < cmdGetArgc(req) )
		{
			if( c < 3 ) // skip the first argument
				jsonBuf[jsonPtr++] = ',';
			
			int len = cmdArgLen(req);
			char buf[len+1];
			buf[len] = 0;
			
			cmdPopArg(req, buf, len);
			
			if( jsonPtr + 20 + len > sizeof(jsonBuf) )
			{
				errorResponse(connData, 500, "Response too large!");
				return HTTPD_CGI_DONE;
			}
			
			WebValueType type = (WebValueType)buf[0];
			
			int nameLen = os_strlen(buf+1);
			jsonBuf[jsonPtr++] = '"';
			os_memcpy(jsonBuf + jsonPtr, buf + 1, nameLen);
			jsonPtr += nameLen;
			jsonBuf[jsonPtr++] = '"';
			jsonBuf[jsonPtr++] = ':';
			
			char * value = buf + 2 + nameLen;
			
			switch(type)
			{
				case WEB_NULL:
					os_memcpy(jsonBuf + jsonPtr, "null", 4);
					jsonPtr += 4;
					break;
				case WEB_INTEGER:
					{
						int v;
						os_memcpy( &v, value, 4);
						
						char intbuf[20];
						os_sprintf(intbuf, "%d", v);
						os_strcpy(jsonBuf + jsonPtr, intbuf);
						jsonPtr += os_strlen(intbuf);
					}
					break;
				case WEB_BOOLEAN:
					if( value ) {
						os_memcpy(jsonBuf + jsonPtr, "true", 4);
						jsonPtr += 4;
					} else {
						os_memcpy(jsonBuf + jsonPtr, "false", 5);
						jsonPtr += 5;
					}
					break;
				case WEB_FLOAT:
					{
						float f;
						os_memcpy( &f, value, 4);
						
						char intbuf[20];
						os_sprintf(intbuf, "%f", f);
						os_strcpy(jsonBuf + jsonPtr, intbuf);
						jsonPtr += os_strlen(intbuf);
					}
					break;
				case WEB_STRING:
					jsonBuf[jsonPtr++] = '"';
					while(*value)
					{
						if( *value == '\\' || *value == '"' )
							jsonBuf[jsonPtr++] = '\\';
						jsonBuf[jsonPtr++] = *(value++);
					}
					jsonBuf[jsonPtr++] = '"';
					break;
				case WEB_JSON:
					os_memcpy(jsonBuf + jsonPtr, value, len - 2 - nameLen);
					jsonPtr += len - 2 - nameLen;
					break;
			}
		}
		
		jsonBuf[jsonPtr++] = '}';
		
		noCacheHeaders(connData, 200);
		httpdHeader(connData, "Content-Type", "application/json");
		
		char cl[16];
		os_sprintf(cl, "%d", jsonPtr);
		httpdHeader(connData, "Content-Length", cl);
		httpdEndHeaders(connData);
		
		httpdSend(connData, jsonBuf, jsonPtr);
		return HTTPD_CGI_DONE;
	}
	
	return HTTPD_CGI_MORE;
}

void ICACHE_FLASH_ATTR WEB_JsonData(CmdPacket *cmd)
{
	CmdRequest req;
	cmdRequest(&req, cmd);
	
	if (cmdGetArgc(&req) < 3) return;
	
	uint8_t ip[4];
	cmdPopArg(&req, ip, 4);
	
	uint16_t port;
	cmdPopArg(&req, &port, 2);
	
	httpdNotify(ip, port, &req);
}
