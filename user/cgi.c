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
#include "../include/httpdconfig.h"


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
int ICACHE_FLASH_ATTR tplLed(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	if (token==NULL) return HTTPD_CGI_DONE;

	os_strcpy(buff, "Unknown");
	if (os_strcmp(token, "ledstate")==0) {
		if (currLedState) {
			os_strcpy(buff, "on");
		} else {
			os_strcpy(buff, "off");
		}
	}
	httpdSend(connData, buff, -1);
	return HTTPD_CGI_DONE;
}

static long hitCounter=0;

//Template code for the counter on the index page.
int ICACHE_FLASH_ATTR tplCounter(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	if (token==NULL) return HTTPD_CGI_DONE;

	if (os_strcmp(token, "counter")==0) {
		hitCounter++;
		os_sprintf(buff, "%ld", hitCounter);
	}
	httpdSend(connData, buff, -1);
	return HTTPD_CGI_DONE;
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

uint32_t postCounter = 0;

int ICACHE_FLASH_ATTR cgiUploadEspfs(HttpdConnData *connData) {
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}
	SpiFlashOpResult ret;
	int x;
	uint32_t flashOff = ESPFS_POS;
	uint32_t flashSize = ESPFS_SIZE;
	
	//If this is the first time, erase the flash sector
	if (postCounter == 0){
		os_printf("Erasing flash at 0x%x...\n", flashOff);
		// Which segment are we flashing?	
		for (x=0; x<flashSize; x+=4096){
			spi_flash_erase_sector((flashOff+x)/0x1000);
		}
		os_printf("Done erasing.\n");
	}
	
	// Because we get sent 1k chunks until the end, if data is less than 1k, we pad it as it must be the end...right???
	if (connData->postBuffSize==1024){
		ret=spi_flash_write((flashOff + postCounter), (uint32 *)connData->postBuff, 1024);
		os_printf("Flash return %d\n", ret);
	} else {
		// Think we can probably use postReceived to check if it's the last chunk and then pad the original postBuff to avoid allocating another 1k of memory
		char *postBuff = (char*)os_zalloc(1024);
                os_printf("Mallocced buffer of 1024 bytes of last chunk.\n");
		os_memcpy(postBuff, connData->postBuff, connData->postBuffSize);
		ret=spi_flash_write((flashOff + postCounter), (uint32 *)postBuff, 1024);
		os_printf("Flash return %d\n", ret);
	}
	
	// Count bytes for data
	postCounter = postCounter + connData->postBuffSize;//connData->postBuff);
	os_printf("Wrote %d bytes (%dB of %d)\n", connData->postBuffSize, postCounter, connData->postLen);//&connData->postBuff));

	if (postCounter == connData->postLen){
		httpdSend(connData, "Finished uploading", -1);
		postCounter=0;
		return HTTPD_CGI_DONE;
	} else {
		return HTTPD_CGI_MORE;
	}
}
