// Copyright (c) 2015 by Thorsten von Eicken, see LICENSE.txt in the esp-link repo

#include <esp8266.h>
#include <osapi.h>
#include "cgi.h"
#include "cgioptiboot.h"
#include "multipart.h"
#include "espfsformat.h"
#include "config.h"
#include "web-server.h"

int upload_offset = 0;    // flash offset where to store page upload
int html_header_len = 0;  // HTML header length (for uploading HTML files)

// this is the header to add if user uploads HTML file
const char * HTML_HEADER =   "<!doctype html><html><head><title>esp-link</title>"
                             "<link rel=stylesheet href=\"/pure.css\"><link rel=stylesheet href=\"/style.css\">"
                             "<meta name=viewport content=\"width=device-width, initial-scale=1\"><script src=\"/ui.js\">"
                             "</script><script src=\"/userpage.js\"></script></head><body><div id=layout>    ";

// multipart callback for uploading user defined pages
int ICACHE_FLASH_ATTR webServerSetupMultipartCallback(MultipartCmd cmd, char *data, int dataLen, int position)
{
  switch(cmd)
  {
    case FILE_START:
      upload_offset = 0;
      html_header_len = 0;
      // simple HTML file
      if( ( dataLen > 5 ) && ( os_strcmp(data + dataLen - 5, ".html") == 0 ) ) // if the file ends with .html, wrap into an espfs image
      {
        // write the start block on esp-fs
        int spi_flash_addr = getUserPageSectionStart();
        spi_flash_erase_sector(spi_flash_addr/SPI_FLASH_SEC_SIZE);
        EspFsHeader hdr;
        hdr.magic = 0xFFFFFFFF; // espfs magic is invalid during upload
        hdr.flags = 0;
        hdr.compression = 0;
        
        int len = dataLen + 1;
        while(( len & 3 ) != 0 )
          len++;
        
        hdr.nameLen = len;
        hdr.fileLenComp = hdr.fileLenDecomp = 0xFFFFFFFF;

        spi_flash_write( spi_flash_addr + upload_offset, (uint32_t *)(&hdr), sizeof(EspFsHeader) );
        upload_offset += sizeof(EspFsHeader);

        char nameBuf[len];
        os_memset(nameBuf, 0, len);
        os_memcpy(nameBuf, data, dataLen);

        spi_flash_write( spi_flash_addr + upload_offset, (uint32_t *)(nameBuf), len );
        upload_offset += len;

        html_header_len = os_strlen(HTML_HEADER) & ~3; // upload only 4 byte aligned part
        char buf[html_header_len];
        os_memcpy(buf, HTML_HEADER, html_header_len);
        spi_flash_write( spi_flash_addr + upload_offset, (uint32_t *)(buf), html_header_len );
        upload_offset += html_header_len;
      }
      break;
    case FILE_DATA:
      if(( position < 4 ) && (upload_offset == 0)) // for espfs images check the magic number
      {
        for(int p = position; p < 4; p++ )
        {
          if( data[p - position] != ((ESPFS_MAGIC >> (p * 8) ) & 255 ) )
          {
            os_printf("Not an espfs image!\n");
            return 1;
          }
          data[p - position] = 0xFF;  // espfs magic is invalid during upload
        }
      }
      
      int spi_flash_addr = getUserPageSectionStart() + upload_offset + position;
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
        if( html_header_len != 0 )
        {
          // write the terminating block on esp-fs
          int spi_flash_addr = getUserPageSectionStart() + upload_offset + position;

          uint32_t pad = 0;
          uint8_t pad_cnt = (4 - position) & 3;
          if( pad_cnt )
            spi_flash_write( spi_flash_addr, &pad, pad_cnt );
	  
	  spi_flash_addr += pad_cnt;

	  // create ESPFS image
          EspFsHeader hdr;
          hdr.magic = ESPFS_MAGIC; 
          hdr.flags = 1;
          hdr.compression = 0;
          hdr.nameLen = 0;
          hdr.fileLenComp = hdr.fileLenDecomp = 0;

          spi_flash_write( spi_flash_addr, (uint32_t *)(&hdr), sizeof(EspFsHeader) );

          uint32_t totallen = html_header_len + position;
  
	  // restore ESPFS magic
          spi_flash_write( (int)getUserPageSectionStart(), (uint32_t *)&hdr.magic, sizeof(uint32_t) );
	  // set file size
          spi_flash_write( (int)getUserPageSectionStart() + 8, &totallen, sizeof(uint32_t) );
          spi_flash_write( (int)getUserPageSectionStart() + 12, &totallen, sizeof(uint32_t) );
        }
        else
        {
	  // set espfs magic (set it valid)
          uint32_t magic = ESPFS_MAGIC;
          spi_flash_write( (int)getUserPageSectionStart(), (uint32_t *)&magic, sizeof(uint32_t) );
	}
        WEB_Init(); // reload the content
      }
      break;
  }
  return 0;
}

MultipartCtx * webServerContext = NULL; // multipart upload context for web server

// this callback is called when user uploads the web-page
int ICACHE_FLASH_ATTR cgiWebServerSetupUpload(HttpdConnData *connData)
{
  if( webServerContext == NULL )
    webServerContext = multipartCreateContext( webServerSetupMultipartCallback );
  
  return multipartProcess(webServerContext, connData);
}
