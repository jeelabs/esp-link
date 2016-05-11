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
					// TODO
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
	
	// TODO
	return HTTPD_CGI_MORE;
}

void ICACHE_FLASH_ATTR WEB_JsonData(CmdPacket *cmd)
{
}