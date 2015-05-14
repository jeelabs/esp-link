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

// Return which firmware needs to be uploaded next
int ICACHE_FLASH_ATTR cgiGetFirmwareNext(HttpdConnData *connData) {
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	uint8 id = system_upgrade_userbin_check();
	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Content-Type", "text/plain");
	httpdEndHeaders(connData);
	httpdSend(connData, (id == 1 ? "user1.bin" : "user2.bin"), -1);
	os_printf("Next firmware: user%d.bin\n", 1-id);

	return HTTPD_CGI_DONE;
}

//Cgi that allows the firmware to be replaced via http POST
int ICACHE_FLASH_ATTR cgiUploadFirmware(HttpdConnData *connData) {
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	if(connData->post->len > FIRMWARE_SIZE){
		// The uploaded file is too large
		os_printf("Firmware image too large\n");
		httpdStartResponse(connData, 400);
		httpdHeader(connData, "Content-Type", "text/plain");
		httpdEndHeaders(connData);
		httpdSend(connData, "Firmware image loo large.\r\n", -1);
		return HTTPD_CGI_DONE;
	}

	uint8 id = system_upgrade_userbin_check();

	int address;
	if (id == 1) {
		address = 4*1024; // start after 4KB boot partition
	} else {
		// 4KB boot, firmware1, 16KB user param, 4KB reserved
		address = 4*1024 + FIRMWARE_SIZE + 16*1024 + 4*1024;
	}
	address += connData->post->received - connData->post->buffLen;

	if(address % SPI_FLASH_SEC_SIZE == 0){
		// We need to erase this block
		os_printf("Erasing flash at 0x%05x (id=%d)\n", address, 2-id);
		spi_flash_erase_sector(address/SPI_FLASH_SEC_SIZE);
	}

	// Write the data
	os_printf("Writing %d bytes at 0x%05x (%d of %d)\n", connData->post->buffSize, address,
			connData->post->received, connData->post->len);
	spi_flash_write(address, (uint32 *)connData->post->buff, connData->post->buffLen);

	if (connData->post->received == connData->post->len){
		// TODO: verify the firmware (is there a checksum or something???)
		httpdStartResponse(connData, 200);
		httpdEndHeaders(connData);
		return HTTPD_CGI_DONE;
	} else {
		return HTTPD_CGI_MORE;
	}
}

// Handle request to reboot into the new firmware
int ICACHE_FLASH_ATTR cgiRebootFirmware(HttpdConnData *connData) {
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	// TODO: sanity-check that the 'next' partition actually contains something that looks like
	// valid firmware

	// This hsould probably be forked into a separate task that waits a second to let the
	// current HTTP request finish...
	system_upgrade_flag_set(UPGRADE_FLAG_FINISH);
	system_upgrade_reboot();
	httpdStartResponse(connData, 200);
	httpdEndHeaders(connData);
	return HTTPD_CGI_DONE;
}


