#include <esp8266.h>
#include <osapi.h>

#include "multipart.h"
#include "cgi.h"

#define BOUNDARY_SIZE 128

typedef enum {
  STATE_SEARCH_BOUNDARY = 0, // state: searching multipart boundary
  STATE_SEARCH_HEADER,       // state: search multipart file header
  STATE_SEARCH_HEADER_END,   // state: search the end of the file header
  STATE_UPLOAD_FILE,         // state: read file content
  STATE_ERROR,               // state: error (stop processing)
} MultipartState;

struct _MultipartCtx {
  MultipartCallback callBack;           // callback for multipart events
  int               position;           // current file position
  int               startTime;          // timestamp when connection was initiated
  int               recvPosition;       // receive position (how many bytes was processed from the HTTP post)
  char *            boundaryBuffer;     // buffer used for boundary detection
  int               boundaryBufferPtr;  // pointer in the boundary buffer
  MultipartState    state;              // multipart processing state
};

// this method is responsible for creating the multipart context
MultipartCtx * ICACHE_FLASH_ATTR multipartCreateContext(MultipartCallback callback)
{
  MultipartCtx * ctx = (MultipartCtx *)os_malloc(sizeof(MultipartCtx));
  ctx->callBack = callback;
  ctx->position = ctx->startTime = ctx->recvPosition = ctx->boundaryBufferPtr = 0;
  ctx->boundaryBuffer = NULL;
  ctx->state = STATE_SEARCH_BOUNDARY;
  return ctx;
}

// for allocating buffer for multipart upload 
void ICACHE_FLASH_ATTR multipartAllocBoundaryBuffer(MultipartCtx * context)
{
  if( context->boundaryBuffer == NULL )
    context->boundaryBuffer = (char *)os_malloc(3*BOUNDARY_SIZE + 1);
  context->boundaryBufferPtr = 0;
}

// for freeing multipart buffer
void ICACHE_FLASH_ATTR multipartFreeBoundaryBuffer(MultipartCtx * context)
{
  if( context->boundaryBuffer != NULL )
  {
    os_free(context->boundaryBuffer);
    context->boundaryBuffer = NULL;
  }
}

// for destroying the context
void ICACHE_FLASH_ATTR multipartDestroyContext(MultipartCtx * context)
{
  multipartFreeBoundaryBuffer(context);
  os_free(context);
}

// this is because of os_memmem is missing
void * mp_memmem(const void *l, size_t l_len, const void *s, size_t s_len)
{
	register char *cur, *last;
	const char *cl = (const char *)l;
	const char *cs = (const char *)s;

	/* we need something to compare */
	if (l_len == 0 || s_len == 0)
		return NULL;

	/* "s" must be smaller or equal to "l" */
	if (l_len < s_len)
		return NULL;

	/* special case where s_len == 1 */
	if (s_len == 1)
		return memchr(l, (int)*cs, l_len);

	/* the last position where its possible to find "s" in "l" */
	last = (char *)cl + l_len - s_len;

	for (cur = (char *)cl; cur <= last; cur++)
		if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
			return cur;

	return NULL;
}


// this method is for processing data coming from the HTTP post request
//   context:   the multipart context
//   boundary:  a string which indicates boundary
//   data:      the received data
//   len:       the received data length (can't be bigger than BOUNDARY_SIZE)
//   last:      last packet indicator
//
// Detecting a boundary is not easy. One has to take care of boundaries which are splitted in 2 packets
//   [Packet 1, 5 bytes of the boundary][Packet 2, remaining 10 bytes of the boundary];
//
// Algorythm:
//   - create a buffer which size is 3*BOUNDARY_SIZE
//   - put data into the buffer as long as the buffer size is smaller than 2*BOUNDARY_SIZE
//   - search boundary in the received buffer, if found: boundary reached -> process data before boundary -> process boundary
//   - if not found -> process the first BOUNDARY_SIZE amount of bytes from the buffer
//   - remove processed data from the buffer
// this algorythm guarantees that no boundary loss will happen

int ICACHE_FLASH_ATTR multipartProcessData(MultipartCtx * context, char * boundary, char * data, int len, int last)
{
  if( len != 0 ) // add data to the boundary buffer
  {
    os_memcpy(context->boundaryBuffer + context->boundaryBufferPtr, data, len);
    
    context->boundaryBufferPtr += len;
    context->boundaryBuffer[context->boundaryBufferPtr] = 0;
  }
  
  while( context->boundaryBufferPtr > 0 )
  {
    if( ! last && context->boundaryBufferPtr <= 2 * BOUNDARY_SIZE ) // return if buffer is too small and not the last packet is processed
      return 0;
  
    int dataSize = BOUNDARY_SIZE;
  
    char * boundaryLoc = mp_memmem( context->boundaryBuffer, context->boundaryBufferPtr, boundary, os_strlen(boundary) );
    if( boundaryLoc != NULL )
    {
      int pos = boundaryLoc - context->boundaryBuffer;
      if( pos > BOUNDARY_SIZE ) // process in the next call
        boundaryLoc = NULL;
      else
        dataSize = pos;
    }
    
    if( dataSize != 0 ) // data to process
    {
      switch( context->state )
      {
        case STATE_SEARCH_HEADER:
        case STATE_SEARCH_HEADER_END:
          {
            char * chr = os_strchr( context->boundaryBuffer, '\n' );
            if( chr != NULL )
            {
	      // chop datasize to contain only one line
              int pos = chr - context->boundaryBuffer + 1;
              if( pos < dataSize ) // if chop smaller than the dataSize, delete the boundary
              {
                dataSize = pos;
                boundaryLoc = NULL; // process boundary next time
              }
              if( context->state == STATE_SEARCH_HEADER_END )
              {
                if( pos == 1 || ( ( pos == 2 ) && ( context->boundaryBuffer[0] == '\r' ) ) ) // empty line?
                {
                  context->state = STATE_UPLOAD_FILE;
                  context->position = 0;
                }
              }
              else if( os_strncmp( context->boundaryBuffer, "Content-Disposition:",  20 ) == 0 )
              {
                char * fnam = os_strstr( context->boundaryBuffer, "filename=" );
                if( fnam != NULL )
                {
                  int pos = fnam - context->boundaryBuffer + 9;
                  if( pos < dataSize )
                  {
                    while(context->boundaryBuffer[pos] == ' ') pos++; // skip spaces
                    if( context->boundaryBuffer[pos] == '"' ) // quote start
                    {
                      pos++;
                      int start = pos;
                      while( pos < context->boundaryBufferPtr )
                      {
                        if( context->boundaryBuffer[pos] == '"' ) // quote end
                          break;
                        pos++;
                      }
                      if( pos < context->boundaryBufferPtr )
                      {
                        context->boundaryBuffer[pos] = 0; // terminating zero for the file name
                        os_printf("Uploading file: %s\n", context->boundaryBuffer + start);
                        if( context->callBack( FILE_START, context->boundaryBuffer + start, pos - start, 0 ) ) // FILE_START callback
			  return 1; // if an error happened
                        context->boundaryBuffer[pos] = '"'; // restore the original quote
                        context->state = STATE_SEARCH_HEADER_END;
                      }
                    }
                  }
                }
              }
            }
          }
          break;
        case STATE_UPLOAD_FILE:
          {
            char c = context->boundaryBuffer[dataSize];
            context->boundaryBuffer[dataSize] = 0; // add terminating zero (for easier handling)
            if( context->callBack( FILE_DATA, context->boundaryBuffer, dataSize, context->position ) ) // FILE_DATA callback
	      return 1;
            context->boundaryBuffer[dataSize] = c;
            context->position += dataSize;
          }
          break;
        default:
          break;
      }
    }
    
    if( boundaryLoc != NULL ) // boundary found?
    {
      dataSize += os_strlen(boundary); // jump over the boundary
      if( context->state == STATE_UPLOAD_FILE )
      {
        if( context->callBack( FILE_DONE, NULL, 0, context->position ) ) // file done callback
	  return 1; // if an error happened
        os_printf("File upload done\n");
      }

      context->state = STATE_SEARCH_HEADER; // search the next header
    }
    
    // move the buffer back with dataSize
    context->boundaryBufferPtr -= dataSize;
    os_memcpy(context->boundaryBuffer, context->boundaryBuffer + dataSize, context->boundaryBufferPtr);
  }
  
  return 0;
}

// for processing multipart requests
int ICACHE_FLASH_ATTR multipartProcess(MultipartCtx * context, HttpdConnData * connData )
{
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  
  if (connData->requestType == HTTPD_METHOD_POST) {
    HttpdPostData *post = connData->post;
    
    if( post->multipartBoundary == NULL )
    {
      errorResponse(connData, 404, "Only multipart POST is supported");
      return HTTPD_CGI_DONE;
    }

    if( connData->startTime != context->startTime )
    {
      // reinitialize, as this is a new request
      context->position = 0;
      context->recvPosition = 0;
      context->startTime = connData->startTime;
      context->state = STATE_SEARCH_BOUNDARY;
 
      multipartAllocBoundaryBuffer(context);
      
      if( context->callBack( FILE_UPLOAD_START, NULL, 0, context->position ) ) // start uploading files
        context->state = STATE_ERROR;
    }

    if( context->state != STATE_ERROR )
    {
      int feed = 0;
      while( feed < post->buffLen )
      {
        int len = post->buffLen - feed;
        if( len > BOUNDARY_SIZE )
          len = BOUNDARY_SIZE;
        if( multipartProcessData(context, post->multipartBoundary, post->buff + feed, len, 0) )
        {
          context->state = STATE_ERROR;
          break;
        }
        feed += len;
      }
    }
    
    context->recvPosition += post->buffLen;
    if( context->recvPosition < post->len )
      return HTTPD_CGI_MORE;

    if( context->state != STATE_ERROR )
    {
      // this is the last package, process the remaining data
      if( multipartProcessData(context, post->multipartBoundary, NULL, 0, 1) )
        context->state = STATE_ERROR;
      else if( context->callBack( FILE_UPLOAD_DONE, NULL, 0, context->position ) ) // done with files
        context->state = STATE_ERROR;
    }
    
    multipartFreeBoundaryBuffer( context );
    
    if( context->state == STATE_ERROR )
      errorResponse(connData, 400, "Invalid file upload!");
    else
    {
      httpdStartResponse(connData, 204);
      httpdEndHeaders(connData);
    }
    return HTTPD_CGI_DONE;
  }
  else {
    errorResponse(connData, 404, "Only multipart POST is supported");
    return HTTPD_CGI_DONE;
  }
}
