// Copyright (c) 2015 by Thorsten von Eicken, see LICENSE.txt in the esp-link repo

#include <esp8266.h>
#include <osapi.h>
#include "cgi.h"
#include "cgioptiboot.h"
#include "multipart.h"

void webServerMultipartCallback(MultipartCmd cmd, char *data, int dataLen, int position)
{
  switch(cmd)
  {
    case FILE_START:
      os_printf("CB: File start: %s\n", data);
      break;
    case FILE_DATA:
      os_printf("CB: Data (%d): %s\n", position, data);
      break;
    case FILE_DONE:
      os_printf("CB: Done\n");
      break;
  }
}

MultipartCtx webServerContext = {.callBack = webServerMultipartCallback, .position = 0, .recvPosition = 0, .startTime = 0, .boundaryBuffer = NULL};

int ICACHE_FLASH_ATTR cgiWebServerUpload(HttpdConnData *connData)
{
  os_printf("WebServer upload\n");
  return multipartProcess(&webServerContext, connData);
}
