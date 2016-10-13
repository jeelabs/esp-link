#include "web-server.h"

#include <espconn.h>

#include "espfs.h"
#include "config.h"
#include "cgi.h"
#include "cmd.h"
#include "serbridge.h"

// the file is responsible for handling user defined web-pages
// - collects HTML files from user image, shows them on the left frame
// - handles JSON data coming from the browser
// - handles SLIP messages coming from MCU

#define MAX_ARGUMENT_BUFFER_SIZE 128
#define HEADER_SIZE 32

uint32_t web_server_cb = 0;

struct ArgumentBuffer
{
	char argBuffer[MAX_ARGUMENT_BUFFER_SIZE];
	int  argBufferPtr;
	int  numberOfArgs;
};

static char* web_server_reasons[] = {
  "load",     // readable name for RequestReason::LOAD
  "refresh",  // readable name for RequestReason::REFRESH
  "button",   // readable name for RequestReason::BUTTON
  "submit"    // readable name for RequestReason::SUBMIT
};

// this variable contains the names of the user defined pages
// this information appears at the left frame below of the built in URL-s
// format:    ,"UserPage1", "/UserPage1.html", "UserPage2", "/UserPage2.html", 
char * webServerPages = NULL;

char * ICACHE_FLASH_ATTR WEB_UserPages()
{
	return webServerPages;
}

// generates the content of webServerPages variable (called at booting/web page uploading)
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
			int nameLen = os_strlen(it.name);
			if( nameLen >= 6 )
			{
				// fetch HTML files
				if( os_strcmp( it.name + nameLen-5, ".html" ) == 0 )
				{
					int slashPos = nameLen - 5;
					
					// chop path and .html from the name
					while( slashPos > 0 && it.name[slashPos-1] != '/' )
						slashPos--;
					
					// here we check buffer overrun
					int maxLen = 10 + os_strlen( it.name ) + (nameLen - slashPos -5);
					if( maxLen >= sizeof(buffer) )
						break;
					
					os_strcat(buffer, ", \"");
					
					int writePos = os_strlen(buffer);
					for( int i=slashPos; i < nameLen-5; i++ )
					  buffer[writePos++] = it.name[i];
					buffer[writePos] = 0; // terminating zero
					
					os_strcat(buffer, "\", \"/");
					os_strcat(buffer, it.name);
					os_strcat(buffer, "\"");
				}
			}
		}
	}
	
	if( webServerPages != NULL )
		os_free( webServerPages );
	
	int len = os_strlen(buffer) + 1;
	webServerPages = (char *)os_malloc( len );
	os_memcpy( webServerPages, buffer, len );
}

// initializer
void ICACHE_FLASH_ATTR WEB_Init()
{
	espFsInit(userPageCtx, (void *)getUserPageSectionStart(), ESPFS_FLASH);
	if( espFsIsValid( userPageCtx ) )
		os_printf("Valid user file system found!\n");
	else
		os_printf("No user file system found!\n");
	WEB_BrowseFiles(); // collect user defined HTML files
}

// initializes the argument buffer
static void ICACHE_FLASH_ATTR WEB_argInit(struct ArgumentBuffer * argBuffer)
{
	argBuffer->numberOfArgs = 0;
	argBuffer->argBufferPtr = 0;
}

// adds an argument to the argument buffer (returns 0 if successful)
static int ICACHE_FLASH_ATTR WEB_addArg(struct ArgumentBuffer * argBuffer, char * arg, int argLen )
{
	if( argBuffer->argBufferPtr + argLen + sizeof(int) >= MAX_ARGUMENT_BUFFER_SIZE )
		return -1; // buffer overflow
	
	os_memcpy(argBuffer->argBuffer + argBuffer->argBufferPtr, &argLen, sizeof(int));
	
	if( argLen != 0 )
	{
		os_memcpy( argBuffer->argBuffer + argBuffer->argBufferPtr + sizeof(int), arg, argLen );
		argBuffer->numberOfArgs++;
	}
	
	argBuffer->argBufferPtr += argLen + sizeof(int);
	return 0;
}

// creates and sends a SLIP message from the argument buffer
static void ICACHE_FLASH_ATTR WEB_sendArgBuffer(struct ArgumentBuffer * argBuffer, HttpdConnData *connData, RequestReason reason)
{
	cmdResponseStart(CMD_RESP_CB, web_server_cb, 4 + argBuffer->numberOfArgs);
	uint16_t r = (uint16_t)reason;
	cmdResponseBody(&r, sizeof(uint16_t));                                      // 1st argument: reason
	cmdResponseBody(&connData->conn->proto.tcp->remote_ip, 4);                  // 2nd argument: IP
	cmdResponseBody(&connData->conn->proto.tcp->remote_port, sizeof(uint16_t)); // 3rd argument: port
	cmdResponseBody(connData->url, os_strlen(connData->url));                   // 4th argument: URL
	
	int p = 0;
	for( int j=0; j < argBuffer->numberOfArgs; j++ )
	{
		int argLen;
		os_memcpy( &argLen, argBuffer->argBuffer + p, sizeof(int) );
		
		char * arg = argBuffer->argBuffer + p + sizeof(int);
		cmdResponseBody(arg, argLen);
		p += argLen + sizeof(int);
	}
	
	cmdResponseEnd();
}

// this method processes SLIP data from MCU and converts to JSON
// this method receives JSON from the browser, sends SLIP data to MCU
static int ICACHE_FLASH_ATTR WEB_handleJSONRequest(HttpdConnData *connData)
{
	if( !flashConfig.slip_enable )
	{
		errorResponse(connData, 400, "Slip processing is disabled!");
		return HTTPD_CGI_DONE;
	}
	
	if( web_server_cb == 0 )
	{
		errorResponse(connData, 500, "No MCU callback is registered!");
		return HTTPD_CGI_DONE;
	}
	if( serbridgeInMCUFlashing() )
	{
		errorResponse(connData, 500, "Slip disabled at uploading program onto the MCU!");
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
	
	struct ArgumentBuffer argBuffer;
	WEB_argInit( &argBuffer );
	
	switch(reason)
	{
		case BUTTON:
			{
				char id_buf[40];
				
				int id_len = httpdFindArg(connData->getArgs, "id", id_buf, sizeof(id_buf));
				if( id_len <= 0 )
				{
					errorResponse(connData, 400, "No button ID specified!");
					return HTTPD_CGI_DONE;
				}
				if( WEB_addArg(&argBuffer, id_buf, id_len) )
				{
					errorResponse(connData, 400, "Post too large!");
					return HTTPD_CGI_DONE;
				}
			}
			break;
		case SUBMIT:
			{
				if( connData->post->received < connData->post->len )
				{
					errorResponse(connData, 400, "Post too large!");
					return HTTPD_CGI_DONE;
				}
				
				int bptr = 0;
				int sent_args = 0;
				int max_buf_size = MAX_ARGUMENT_BUFFER_SIZE - HEADER_SIZE - os_strlen(connData->url);
				
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
						int vblen = os_strlen(val+1) * 2;
						char value[vblen];
						httpdUrlDecode(val+1, strlen(val+1), value, vblen);
						
						int namLen = os_strlen(name);
						int valLen = os_strlen(value);
						
						char arg[namLen + valLen + 3];
						int argPtr = 0;
						arg[argPtr++] = (char)WEB_STRING;
						os_strcpy( arg + argPtr, name );
						argPtr += namLen;
						arg[argPtr++] = 0;
						os_strcpy( arg + argPtr, value );
						argPtr += valLen;
						
						if( sent_args != 0 )
						{
							if( argBuffer.argBufferPtr + argPtr >= max_buf_size )
							{
								WEB_addArg(&argBuffer, NULL, 0); // there's enough room in the buffer for termination block
								WEB_sendArgBuffer(&argBuffer, connData, reason );
								WEB_argInit( &argBuffer );
								sent_args = 0;
							}
						}
						
						if( WEB_addArg(&argBuffer, arg, argPtr) )
						{
							errorResponse(connData, 400, "Post too large!");
							return HTTPD_CGI_DONE;
						}
						sent_args++;
					}
				}
			}
			break;
		case LOAD:
		case REFRESH:
		default:
			break;
	}
	
	if( WEB_addArg(&argBuffer, NULL, 0) )
	{
		errorResponse(connData, 400, "Post too large!");
		return HTTPD_CGI_DONE;
	}
	
	os_printf("Web callback to MCU: %s\n", reasonBuf);
	
	WEB_sendArgBuffer(&argBuffer, connData, reason );
	
	if( reason == SUBMIT )
	{
		httpdStartResponse(connData, 204);
		httpdEndHeaders(connData);
		return HTTPD_CGI_DONE;
	}
	
	return HTTPD_CGI_MORE;
}

// this method receives SLIP data from MCU sends JSON to the browser
static int ICACHE_FLASH_ATTR WEB_handleMCUResponse(HttpdConnData *connData, CmdRequest * response)
{
	char jsonBuf[1500];
	int  jsonPtr = 0;
	
	
	jsonBuf[jsonPtr++] = '{';
	
	int c = 2;
	while( c++ < cmdGetArgc(response) )
	{
		int len = cmdArgLen(response);
		char buf[len+1];
		buf[len] = 0;
		
		cmdPopArg(response, buf, len);
		
		if(len == 0)
			break; // last argument
		
		if( c > 3 ) // skip the first argument
			jsonBuf[jsonPtr++] = ',';
		
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
				if( *value ) {
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
					
					// os_sprintf doesn't support %f
					int intPart = f;
					int fracPart = (f - intPart) * 1000; // use 3 digit precision
					if( fracPart < 0 ) // for negative numbers
						fracPart = -fracPart;
					
					char floatBuf[20];
					os_sprintf(floatBuf, "%d.%03d", intPart, fracPart);
					os_strcpy(jsonBuf + jsonPtr, floatBuf);
					jsonPtr += os_strlen(floatBuf);
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

// this method is responsible for the MCU <==JSON==> Browser communication
int ICACHE_FLASH_ATTR WEB_CgiJsonHook(HttpdConnData *connData)
{
	if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
	
	void * cgiData = connData->cgiData;
	
	if( cgiData == NULL )
	{
		connData->cgiData = (void *)1; // indicate, that request was processed
		return WEB_handleJSONRequest(connData);
	}
	
	if( connData->cgiResponse != NULL ) // data from MCU
		return WEB_handleMCUResponse(connData, (CmdRequest *)(connData->cgiResponse));
	
	return HTTPD_CGI_MORE;
}

// configuring the callback
void ICACHE_FLASH_ATTR WEB_Setup(CmdPacket *cmd)
{
	CmdRequest req;
	cmdRequest(&req, cmd);
	
	if (cmdGetArgc(&req) < 1) return;
	
	cmdPopArg(&req, &web_server_cb, 4); // pop the callback
	
	os_printf("Web-server connected, cb=0x%x\n", web_server_cb);
}

// this method is called when MCU transmits WEB_DATA command
void ICACHE_FLASH_ATTR WEB_Data(CmdPacket *cmd)
{
	CmdRequest req;
	cmdRequest(&req, cmd);
	
	if (cmdGetArgc(&req) < 2) return;
	
	uint8_t ip[4];
	cmdPopArg(&req, ip, 4);    // pop the IP address
	
	uint16_t port;
	cmdPopArg(&req, &port, 2); // pop the HTTP port
	
	HttpdConnData * conn = httpdLookUpConn(ip, port);  // look up connection based on IP/port
	if( conn != NULL && conn->cgi == WEB_CgiJsonHook ) // make sure that the right CGI handler is configured
		httpdSetCGIResponse( conn, &req );
	else
		os_printf("WEB_DATA ignored as no valid HTTP connection found!\n");
}
