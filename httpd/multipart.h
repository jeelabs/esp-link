#ifndef MULTIPART_H
#define MULTIPART_H

#include <httpd.h>

typedef enum {
  FILE_UPLOAD_START, // multipart: uploading files started
  FILE_START,        // multipart: the start of a new file (can be more)
  FILE_DATA,         // multipart: file data
  FILE_DONE,         // multipart: file end
  FILE_UPLOAD_DONE,  // multipart: finished for all files
} MultipartCmd;

// multipart callback
// -> FILE_START : data+dataLen contains the filename, position is 0
// -> FILE_DATA  : data+dataLen contains file data, position is the file position
// -> FILE_DONE  : data+dataLen is 0, position is the complete file size

typedef int (* MultipartCallback)(MultipartCmd cmd, char *data, int dataLen, int position);

struct _MultipartCtx; // the context for multipart listening

typedef struct _MultipartCtx MultipartCtx;

// use this for creating a multipart context
MultipartCtx * ICACHE_FLASH_ATTR multipartCreateContext(MultipartCallback callback);

// for destroying multipart context
void ICACHE_FLASH_ATTR multipartDestroyContext(MultipartCtx * context);

// use this function for processing HTML multipart updates
int ICACHE_FLASH_ATTR multipartProcess(MultipartCtx * context, HttpdConnData * post );

#endif /* MULTIPART_H */
