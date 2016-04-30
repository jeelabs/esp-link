#ifndef MULTIPART_H
#define MULTIPART_H

#include <httpd.h>

typedef enum {
  FILE_START,
  FILE_DATA,
  FILE_DONE,
} MultipartCmd;

typedef enum {
  STATE_SEARCH_BOUNDARY = 0,
  STATE_SEARCH_HEADER,
  STATE_SEARCH_HEADER_END,
  STATE_UPLOAD_FILE
} MultipartState;

typedef void (* MultipartCallback)(MultipartCmd cmd, char *data, int dataLen, int position);

typedef struct {
  MultipartCallback callBack;
  int               position;
  int               startTime;
  int               recvPosition;
  char *            boundaryBuffer;
  int               boundaryBufferPtr;
  MultipartState    state;
} MultipartCtx;

int ICACHE_FLASH_ATTR multipartProcess(MultipartCtx * context, HttpdConnData * post );

#endif /* MULTIPART_H */
