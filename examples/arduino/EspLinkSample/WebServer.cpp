#include "WebServer.h"

WebServer * WebServer::instance = NULL;

void webServerCallback(CmdRequest *req)
{
  WebServer::getInstance()->handleRequest(req);
}

WebServer::WebServer(Stream &streamIn, const WebMethod * PROGMEM methodsIn):espLink(streamIn, webServerCallback),methods(methodsIn),stream(streamIn)
{
  instance = this;
}

void WebServer::init()
{
  registerCallback();
}

void WebServer::loop()
{
  // TODO: resubscribe periodically
  espLink.readLoop();
}

void WebServer::registerCallback()
{
  espLink.sendPacketStart(CMD_CB_ADD, 100, 1);
  espLink.sendPacketArg(5, (uint8_t *)"webCb");
  espLink.sendPacketEnd();
}

void WebServer::invokeMethod(RequestReason reason, WebMethod * method, CmdRequest *req)
{
  switch(reason)
  {
    case WS_BUTTON:
      {
        uint16_t len = espLink.cmdArgLen(req);
        char bf[len+1];
        bf[len] = 0;
        espLink.cmdPopArg(req, bf, len);

        method->callback(BUTTON_PRESS, bf, len);
      }
      break;
    case WS_SUBMIT:
      {
        /* TODO: iterate through fields */
      }
      return;
    default:
      break;
  }

  args_to_send = -1;
  method->callback( reason == WS_LOAD ? LOAD : REFRESH, NULL, 0);

  if( args_to_send == -1 )
  {
    espLink.sendPacketStart(CMD_WEB_JSON_DATA, 100, 2);
    espLink.sendPacketArg(4, remote_ip);
    espLink.sendPacketArg(2, (uint8_t *)&remote_port);
  }
  while( args_to_send-- > 0 )
    espLink.sendPacketArg(0, NULL);
  espLink.sendPacketEnd();
}

void WebServer::handleRequest(CmdRequest *req)
{
  uint16_t shrt;
  espLink.cmdPopArg(req, &shrt, 2);
  RequestReason reason = (RequestReason)shrt;

  espLink.cmdPopArg(req, &remote_ip, 4);
  espLink.cmdPopArg(req, &remote_port, 2);

  {
    uint16_t len = espLink.cmdArgLen(req);
    char bf[len+1];
    bf[len] = 0;
    espLink.cmdPopArg(req, bf, len);

    const WebMethod * meth = methods;
    do
    {
      WebMethod m;
      memcpy_P(&m, meth, sizeof(WebMethod));
      if( m.url == NULL || m.callback == NULL )
        break;

      if( strcmp_P(bf, m.url) == 0 )
      {
        invokeMethod(reason, &m, req);
        return;
      }
      meth++;
    }while(1);
  }

  if( reason == WS_SUBMIT )
    return;

  // empty response
  espLink.sendPacketStart(CMD_WEB_JSON_DATA, 100, 2);
  espLink.sendPacketArg(4, remote_ip);
  espLink.sendPacketArg(2, (uint8_t *)&remote_port);
  espLink.sendPacketEnd();
}

void WebServer::setArgNum(uint8_t num)
{
  espLink.sendPacketStart(CMD_WEB_JSON_DATA, 100, 2 + (args_to_send = num));
  espLink.sendPacketArg(4, remote_ip);
  espLink.sendPacketArg(2, (uint8_t *)&remote_port);
}

void WebServer::setArgString(const char * name, const char * value)
{
  if( args_to_send <= 0 )
    return;
    
  uint8_t nlen = strlen(name);
  uint8_t vlen = strlen(value);
  char buf[nlen + vlen + 3];
  buf[0] = WEB_STRING;
  strcpy(buf+1, name);
  strcpy(buf+2+nlen, value);
  espLink.sendPacketArg(nlen+vlen+2, (uint8_t *)buf);
  
  args_to_send--;
}

void WebServer::setArgStringP(const char * name, const char * value)
{
  if( args_to_send <= 0 )
    return;
    
  uint8_t nlen = strlen(name);
  uint8_t vlen = strlen_P(value);
  char buf[nlen + vlen + 3];
  buf[0] = WEB_STRING;
  strcpy(buf+1, name);
  strcpy_P(buf+2+nlen, value);
  espLink.sendPacketArg(nlen+vlen+2, (uint8_t *)buf);

  args_to_send--;
}

