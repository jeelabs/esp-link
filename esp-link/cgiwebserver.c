// Copyright (c) 2015 by Thorsten von Eicken, see LICENSE.txt in the esp-link repo

#include <esp8266.h>
#include <osapi.h>
#include "cgi.h"
#include "cgioptiboot.h"
#include "multipart.h"
#include "espfsformat.h"

int webServerMultipartCallback(MultipartCmd cmd, char *data, int dataLen, int position)
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
      // TODO: flash write
      break;
    case FILE_DONE:
      // TODO: finalize changes, set back espfs magic
      break;
  }
  return 0;
}

MultipartCtx webServerContext = {.callBack = webServerMultipartCallback, .position = 0, .recvPosition = 0, .startTime = 0, .boundaryBuffer = NULL};

int ICACHE_FLASH_ATTR cgiWebServerUpload(HttpdConnData *connData)
{
  return multipartProcess(&webServerContext, connData);
}
