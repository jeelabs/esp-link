// Copyright (c) 2015 by Thorsten von Eicken, see LICENSE.txt in the esp-link repo

#include <esp8266.h>
#include <osapi.h>
#include "cgi.h"
#include "cgioptiboot.h"
#include "multipart.h"
#include "espfsformat.h"
#include "config.h"
#include "web-server.h"

int header_position = 0;  // flash offset of the file header
int upload_position = 0;  // flash offset where to store page upload
int html_header_len = 0;  // length of the HTML header added to the file

// this is the header to add if user uploads HTML file
const char * HTML_HEADER =   "<!doctype html><html><head><title>esp-link</title>"
                             "<link rel=stylesheet href=\"/pure.css\"><link rel=stylesheet href=\"/style.css\">"
                             "<meta name=viewport content=\"width=device-width, initial-scale=1\"><script src=\"/ui.js\">"
                             "</script><script src=\"/userpage.js\"></script></head><body><div id=layout>    ";

// this method is for flash writing and erasing the page
// write is incremental, so whenever a page border is reached, the next page will be erased
int ICACHE_FLASH_ATTR webServerSetupWriteFlash( int addr, void * data, int length )
{
  int end_addr = addr + length;
  if( end_addr >= getUserPageSectionEnd() )
  {
    os_printf("No more space in the flash!\n");
    return 1;
  }

  void * free_ptr = 0;
  if(( length & 3 ) != 0 ) // ESP8266 always writes 4 bytes, so the remaining ones should be oxFF-ed out
  {
    free_ptr = os_malloc(length + 4);
    os_memset(free_ptr, 0xFF, length + 4);
    os_memcpy(free_ptr, data, length);
    data = free_ptr;
  }

  int ptr = 0;
  while( addr < end_addr )
  {
    if (addr % SPI_FLASH_SEC_SIZE == 0){
      spi_flash_erase_sector(addr/SPI_FLASH_SEC_SIZE);
    }

    int max = (addr | (SPI_FLASH_SEC_SIZE - 1)) + 1;
    int len = end_addr - addr;
    if( end_addr > max )
      len = max - addr;

    spi_flash_write( addr, (uint32_t *)((char *)data + ptr), len );
    ptr += len;
    addr += len;
  }
  if( free_ptr != 0 )
    os_free(free_ptr);
  return 0;
}

// debug code
void ICACHE_FLASH_ATTR dumpFlash( int end )
{
  int dump = getUserPageSectionStart();
  while( dump < end )
  {
    char buffer[16];
    spi_flash_read(dump, (uint32_t *)buffer, sizeof(buffer));
    char dmpstr[sizeof(buffer)*3];
    os_sprintf(dmpstr, "%06X: ", dump);
    for(int i=0; i < sizeof(buffer); i++ )
      os_sprintf(dmpstr + os_strlen(dmpstr), "%02X ", buffer[i]);
    os_printf("%s\n", dmpstr);
    dump += sizeof(buffer);
  }
}

// multipart callback for uploading user defined pages
int ICACHE_FLASH_ATTR webServerSetupMultipartCallback(MultipartCmd cmd, char *data, int dataLen, int position)
{
  switch(cmd)
  {
    case FILE_UPLOAD_START:
      upload_position = getUserPageSectionStart();
      header_position = upload_position;
      break;
    case FILE_START:
      {
        html_header_len = 0;

        // write the starting block on esp-fs
        EspFsHeader hdr;
        hdr.magic = 0xFFFFFFFF; // espfs magic is invalid during upload
        hdr.flags = 0;
        hdr.compression = 0;

        int len = dataLen + 1;
        while(( len & 3 ) != 0 )
          len++;

        hdr.nameLen = len;
        hdr.fileLenComp = hdr.fileLenDecomp = 0xFFFFFFFF;

        header_position = upload_position;
        if( webServerSetupWriteFlash( upload_position, (uint32_t *)(&hdr), sizeof(EspFsHeader) ) )
          return 1;
        upload_position += sizeof(EspFsHeader);
      
        char nameBuf[len];
        os_memset(nameBuf, 0, len);
        os_memcpy(nameBuf, data, dataLen);

        if( webServerSetupWriteFlash( upload_position, (uint32_t *)(nameBuf), len ) )
          return 1;
        upload_position += len;
      
        // add header to HTML files
        if( ( dataLen > 5 ) && ( os_strcmp(data + dataLen - 5, ".html") == 0 ) ) // if the file ends with .html, wrap into an espfs image
        {
          html_header_len = os_strlen(HTML_HEADER) & ~3; // upload only 4 byte aligned part
          char buf[html_header_len];
          os_memcpy(buf, HTML_HEADER, html_header_len);
          if( webServerSetupWriteFlash( upload_position, (uint32_t *)(buf), html_header_len ) )
            return 1;
          upload_position += html_header_len;
        }
      }
      break;
    case FILE_DATA:
      if( webServerSetupWriteFlash( upload_position, data, dataLen ) )
        return 1;
      upload_position += dataLen;
      break;
    case FILE_DONE:
      {
        // write padding after the file
        uint8_t pad_cnt = (4 - position) & 3;
        if( pad_cnt ) {
          uint32_t pad = 0;
          if( webServerSetupWriteFlash( upload_position, &pad, pad_cnt ) )
            return 1;
          upload_position += pad_cnt;
        }

        EspFsHeader hdr;
        hdr.magic = ESPFS_MAGIC; 
        hdr.fileLenComp = hdr.fileLenDecomp = position + html_header_len;

        // restore ESPFS magic
        spi_flash_write( header_position + ((char *)&hdr.magic - (char*)&hdr), (uint32_t *)&hdr.magic, sizeof(uint32_t) );
        // set file size
        spi_flash_write( header_position + ((char *)&hdr.fileLenComp - (char*)&hdr), (uint32_t *)&hdr.fileLenComp, sizeof(uint32_t) );
        spi_flash_write( header_position + ((char *)&hdr.fileLenDecomp - (char*)&hdr), (uint32_t *)&hdr.fileLenDecomp, sizeof(uint32_t) );
      }
      break;
    case FILE_UPLOAD_DONE:
      {
        // write the termination block

        EspFsHeader hdr;
        hdr.magic = ESPFS_MAGIC; 
        hdr.flags = 1;
        hdr.compression = 0;
        hdr.nameLen = 0;
        hdr.fileLenComp = hdr.fileLenDecomp = 0;

        if( webServerSetupWriteFlash( upload_position, (uint32_t *)(&hdr), sizeof(EspFsHeader) ) )
          return 1;
        upload_position += sizeof(EspFsHeader);

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
