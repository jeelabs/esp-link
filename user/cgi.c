/*
Some random cgi routines. Used in the LED example and the page that returns the entire
flash as a binary. Also handles the hit counter on the main page.
*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


#include <string.h>
#include <osapi.h>
#include "user_interface.h"
#include "mem.h"
#include "httpd.h"
#include "cgi.h"
#include "io.h"
#include <ip_addr.h>
#include "espmissingincludes.h"


//cause I can't be bothered to write an ioGetLed()
static char currLedState=0;

//Cgi that turns the LED on or off according to the 'led' param in the POST data
int ICACHE_FLASH_ATTR cgiLed(HttpdConnData *connData) {
	int len;
	char buff[1024];
	
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	len=httpdFindArg(connData->postBuff, "led", buff, sizeof(buff));
	if (len!=0) {
		currLedState=atoi(buff);
		ioLed(currLedState);
	}

	httpdRedirect(connData, "led.tpl");
	return HTTPD_CGI_DONE;
}



//Template code for the led page.
void ICACHE_FLASH_ATTR tplLed(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	if (token==NULL) return;

	os_strcpy(buff, "Unknown");
	if (os_strcmp(token, "ledstate")==0) {
		if (currLedState) {
			os_strcpy(buff, "on");
		} else {
			os_strcpy(buff, "off");
		}
	}
	httpdSend(connData, buff, -1);
}

static long hitCounter=0;

//Template code for the counter on the index page.
void ICACHE_FLASH_ATTR tplCounter(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	if (token==NULL) return;

	if (os_strcmp(token, "counter")==0) {
		hitCounter++;
		os_sprintf(buff, "%ld", hitCounter);
	}
	httpdSend(connData, buff, -1);
}


//Cgi that reads the SPI flash. Assumes 512KByte flash.
int ICACHE_FLASH_ATTR cgiReadFlash(HttpdConnData *connData) {
	int *pos=(int *)&connData->cgiData;
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	if (*pos==0) {
		os_printf("Start flash download.\n");
		httpdStartResponse(connData, 200);
		httpdHeader(connData, "Content-Type", "application/bin");
		httpdEndHeaders(connData);
		*pos=0x40200000;
		return HTTPD_CGI_MORE;
	}
	//Send 1K of flash per call. We will get called again if we haven't sent 512K yet.
	espconn_sent(connData->conn, (uint8 *)(*pos), 1024);
	*pos+=1024;
	if (*pos>=0x40200000+(512*1024)) return HTTPD_CGI_DONE; else return HTTPD_CGI_MORE;
}

#define LOOKING_FOR_SECTION 1
#define CHECKING_HEADERS 2
#define IN_DATA 3

typedef struct updateState_t {
	char delimiter[61];
	int  step;
} updateState_t;

char* bin_strstr(char *haystack, char *needle, int haystackLen, int needleLen){
	if(needleLen < 0){
		needleLen = strlen(needle);
	}
	char * end = haystack + haystackLen;
	for(; haystack < end; haystack++){
		if(*haystack == *needle){
			int match = true;
			for(int i = 1; i< needleLen; i++){
				if(*(needle + i) != *(haystack + i)){
					match = false;
					break;
				}
			}
			if(match){
				return haystack;
			}
		}
	}
	return NULL;
}


//Cgi to allow user to upload a new espfs image
int ICACHE_FLASH_ATTR updateWeb(HttpdConnData *connData) {
	os_printf("data size   : %d\r\n", connData->postBuffLen);
	updateState_t *state;
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}
	if(connData->requestType == GET){
		httpdStartResponse(connData, 200);
		httpdHeader(connData, "Content-Length", "135");
		httpdEndHeaders(connData);
		httpdSend(connData, "<html><body><form method=\"POST\" enctype=\"multipart/form-data\"><input type=\"file\" name=\"file\"><input type=\"submit\"></form></body></html>", 135);
	}else if(connData->requestType == POST){
		if(connData->postReceived == connData->postBuffLen){
			//it's the first time this handler has been called for this request
			connData->cgiPrivData = (int*)os_malloc(sizeof(updateState_t));
			state = connData->cgiPrivData;
			state->step = LOOKING_FOR_SECTION;
		}else{
			state = connData->cgiPrivData;
		}

		char *b;
		char *p;
		char * end = connData->postBuff + connData->postBuffLen;
		for(b = connData->postBuff; b < end; b++){
			if(state->step == LOOKING_FOR_SECTION){
				if((p = bin_strstr(b, connData->multipartBoundary, connData->postBuffLen - (b - connData->postBuff), -1)) != NULL){
					os_printf("Found section\r\n");
					// We've found one of the sections, now make sure it's the file
					b = p + strlen(connData->multipartBoundary) + 2;
					state->step = CHECKING_HEADERS;
				}else{
					os_printf("Not Found section\r\n");
					break;
				}
			}

			if(state->step == CHECKING_HEADERS){
				//os_printf("next data: %s\n", b);
				if(!os_strncmp(b, "Content-Disposition: form-data", 30)){
					os_printf("Correct header\r\n");
					// It's the Content-Disposition header
					// Find the end of the line
					p = os_strstr(b, "\r\n");
					// turn the end of the line into a string end so we can search within the line
					*p = 0;
					if(os_strstr(b, "name=\"file\"") != NULL){
						os_printf("Correct file\r\n");
						b = p + 2;
						// it's the correct section, skip to the data
						if((p = os_strstr(b, "\r\n\r\n")) != NULL){
							os_printf("Skipping to data\r\n");
							b = p + 4;
							state->step = IN_DATA;
						}else{
							os_printf("Couldn't find line endings\r\n");
							os_printf("data: %s\n", b);
							return HTTPD_CGI_DONE;
						}
					}else{
						// it's the wrong section, skip to the next boundary
						b = p + 1;
						state->step = LOOKING_FOR_SECTION;
					}
				}else{
					// Skip to the next header
					p = os_strstr(b, "\r\n");
					b = p + 2;
				}
			}

			if(state->step == IN_DATA){
				//os_printf("In data\r\n");
				// Make sure it doesn't contain the boundary, but we can't rely on there being no zeroes so can't use strstr
				if((p = bin_strstr(b, connData->multipartBoundary, connData->postBuffLen - (b - connData->postBuff), -1)) == NULL){
					// all of the data is file data
				}else{
					if(b < (p - 2)){ // -2 bytes for the \r\n
						// This is a byte that's part of the file
						os_printf("%c", *b);
					}else{
						os_printf("\r\nComplete\r\n");
					}
				}
			}
		}
		/*
		os_printf("postReceived: %d\r\n", connData->postReceived);
		os_printf("postLen     : %d\r\n", connData->postLen);
		os_printf("data size   : %d\r\n", connData->postBuffLen);
		*/
		if(connData->postReceived >= connData->postLen){
			httpdStartResponse(connData, 204);
			if (connData->cgiPrivData!=NULL) os_free(connData->cgiPrivData);
			return HTTPD_CGI_DONE;
		}else{
			return HTTPD_CGI_NOTDONE;
		}
	}
	return HTTPD_CGI_DONE;
}

