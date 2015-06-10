/*
Some flash handling cgi routines. Used for reading the existing flash and updating the ESPFS image.
*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


#include <esp8266.h>
#include "cgiflash.h"
#include "espfs.h"


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


//Cgi that allows the ESPFS image to be replaced via http POST
int ICACHE_FLASH_ATTR cgiUploadEspfs(HttpdConnData *connData) {
	const CgiUploadEspfsParams *up=(CgiUploadEspfsParams*)connData->cgiArg;

	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	if(connData->post->len > up->espFsSize){
		// The uploaded file is too large
		os_printf("ESPFS file too large\n");
		httpdSend(connData, "HTTP/1.0 500 Internal Server Error\r\nServer: esp8266-httpd/0.3\r\nConnection: close\r\nContent-Type: text/plain\r\nContent-Length: 24\r\n\r\nESPFS image loo large.\r\n", -1);
		return HTTPD_CGI_DONE;
	}
	
	// The source should be 4byte aligned, so go ahead and flash whatever we have
	int address = up->espFsPos + connData->post->received - connData->post->buffLen;
	if(address % SPI_FLASH_SEC_SIZE == 0){
		// We need to erase this block
		os_printf("Erasing flash at %d\n", address/SPI_FLASH_SEC_SIZE);
		spi_flash_erase_sector(address/SPI_FLASH_SEC_SIZE);
	}
	// Write the data
	os_printf("Writing at: 0x%x\n", address);
	spi_flash_write(address, (uint32 *)connData->post->buff, connData->post->buffLen);
	os_printf("Wrote %d bytes (%dB of %d)\n", connData->post->buffSize, connData->post->received, connData->post->len);//&connData->postBuff));

	if (connData->post->received == connData->post->len){
		httpdSend(connData, "Finished uploading", -1);
		return HTTPD_CGI_DONE;
	} else {
		return HTTPD_CGI_MORE;
	}
}
