#include <esp8266.h>
#include <osapi.h>

#include "multipart.h"
#include "cgi.h"

#define BOUNDARY_SIZE 100

void multipartAllocBoundaryBuffer(MultipartCtx * context)
{
  if( context->boundaryBuffer == NULL )
    context->boundaryBuffer = (char *)os_malloc(3*BOUNDARY_SIZE + 1);
  context->boundaryBufferPtr = 0;
}

void multipartFreeBoundaryBuffer(MultipartCtx * context)
{
  if( context->boundaryBuffer != NULL )
  {
    os_free(context->boundaryBuffer);
    context->boundaryBuffer = NULL;
  }
}

void multipartProcessBoundaryBuffer(MultipartCtx * context, char * boundary, char * buff, int len, int last)
{
  if( len != 0 )
  {
    os_memcpy(context->boundaryBuffer + context->boundaryBufferPtr, buff, len);
    
    context->boundaryBufferPtr += len;
    context->boundaryBuffer[context->boundaryBufferPtr] = 0;
  }
  
  while( context->boundaryBufferPtr > 0 )
  {
    if( ! last && context->boundaryBufferPtr <= 2 * BOUNDARY_SIZE )
      return;
  
    int dataSize = BOUNDARY_SIZE;
  
    char * loc = os_strstr( context->boundaryBuffer, boundary );
    if( loc != NULL )
    {
      int pos = loc - context->boundaryBuffer;
      if( pos > BOUNDARY_SIZE )
        loc = NULL;
      else
        dataSize = pos;
    }
    
    if( dataSize != 0 )
    {
      switch( context->state )
      {
        case STATE_SEARCH_HEADER:
        case STATE_SEARCH_HEADER_END:
          {
            char * chr = os_strchr( context->boundaryBuffer, '\n' );
            if( chr != NULL )
            {
              int pos = chr - context->boundaryBuffer + 1;
              if( pos < dataSize )
              {
                dataSize = pos;
                loc = NULL; // this is not yet the boundary
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
                    while(context->boundaryBuffer[pos] == ' ') pos++;
                    if( context->boundaryBuffer[pos] == '"' )
                    {
                      pos++;
                      int start = pos;
                      while( pos < context->boundaryBufferPtr )
                      {
                        if( context->boundaryBuffer[pos] == '"' )
                          break;
                        pos++;
                      }
                      if( pos < context->boundaryBufferPtr )
                      {
                        context->boundaryBuffer[pos] = 0;
                        os_printf("Uploading file: %s\n", context->boundaryBuffer + start);
                        context->callBack( FILE_START, context->boundaryBuffer + start, pos - start, 0 );
                        context->boundaryBuffer[pos] = '"';
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
            context->callBack( FILE_DATA, context->boundaryBuffer, dataSize, context->position );
            context->boundaryBuffer[dataSize] = c;
            context->position += dataSize;
          }
          break;
        default:
          break;
      }
    }
    
    if( loc != NULL )
    {
      dataSize += os_strlen(boundary);
      if( context->state == STATE_UPLOAD_FILE )
      {
        context->callBack( FILE_DONE, NULL, 0, context->position );
        os_printf("File upload done\n");
      }

      context->state = STATE_SEARCH_HEADER;
    }
    
    context->boundaryBufferPtr -= dataSize;
    os_memcpy(context->boundaryBuffer, context->boundaryBuffer + dataSize, context->boundaryBufferPtr);
  }
}

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
      // reinitialize, as this is a different request
      context->position = 0;
      context->recvPosition = 0;
      context->startTime = connData->startTime;
      context->state = STATE_SEARCH_BOUNDARY;
 
      multipartAllocBoundaryBuffer(context);
    }

    int feed = 0;
    while( feed < post->buffLen )
    {
      int len = post->buffLen - feed;
      if( len > BOUNDARY_SIZE )
        len = BOUNDARY_SIZE;
      multipartProcessBoundaryBuffer(context, post->multipartBoundary, post->buff + feed, len, 0);
      feed += len;
    }
    
    context->recvPosition += post->buffLen;
    if( context->recvPosition < post->len )
      return HTTPD_CGI_MORE;

    multipartProcessBoundaryBuffer(context, post->multipartBoundary, NULL, 0, 1);
    multipartFreeBoundaryBuffer( context );

    httpdStartResponse(connData, 204);
    httpdEndHeaders(connData);
    return HTTPD_CGI_DONE;
  }
  else {
    errorResponse(connData, 404, "Only multipart POST is supported");
    return HTTPD_CGI_DONE;
  }
}
