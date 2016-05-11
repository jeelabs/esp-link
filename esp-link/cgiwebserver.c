// Copyright (c) 2015 by Thorsten von Eicken, see LICENSE.txt in the esp-link repo

#include <esp8266.h>
#include <osapi.h>
#include "cgi.h"
#include "cgioptiboot.h"
#include "multipart.h"
#include "espfsformat.h"
#include "config.h"
#include "web-server.h"

int ICACHE_FLASH_ATTR webServerMultipartCallback(MultipartCmd cmd, char *data, int dataLen, int position)
{
  switch(cmd)
  {
    case FILE_START:
      // do nothing
      break;
    case FILE_DATA:
      if( position < 4 )
      {
        for(int p = position; p < 4; p++ )
        {
          if( data[p - position] != ((ESPFS_MAGIC >> (p * 8) ) & 255 ) )
          {
            os_printf("Not an espfs image!\n");
            return 1;
          }
          data[p - position] = 0xFF; // clean espfs magic to mark as invalid
        }
      }
      
      int spi_flash_addr = getUserPageSectionStart() + position;
      int spi_flash_end_addr = spi_flash_addr + dataLen;
      if( spi_flash_end_addr + dataLen >= getUserPageSectionEnd() )
      {
        os_printf("No more space in the flash!\n");
        return 1;
      }
      
      int ptr = 0;
      while( spi_flash_addr < spi_flash_end_addr )
      {
        if (spi_flash_addr % SPI_FLASH_SEC_SIZE == 0){
          spi_flash_erase_sector(spi_flash_addr/SPI_FLASH_SEC_SIZE);
        }
        
        int max = (spi_flash_addr | (SPI_FLASH_SEC_SIZE - 1)) + 1;
        int len = spi_flash_end_addr - spi_flash_addr;
        if( spi_flash_end_addr > max )
          len = max - spi_flash_addr;

        spi_flash_write( spi_flash_addr, (uint32_t *)(data + ptr), len );
        ptr += len;
        spi_flash_addr += len;
      }
      
      break;
    case FILE_DONE:
      {
        uint32_t magic = ESPFS_MAGIC;
        spi_flash_write( (int)getUserPageSectionStart(), (uint32_t *)&magic, sizeof(uint32_t) );
	WEB_Init();
      }
      break;
  }
  return 0;
}

MultipartCtx webServerContext = {.callBack = webServerMultipartCallback, .position = 0, .recvPosition = 0, .startTime = 0, .boundaryBuffer = NULL};

int ICACHE_FLASH_ATTR cgiWebServerUpload(HttpdConnData *connData)
{
  return multipartProcess(&webServerContext, connData);
}
