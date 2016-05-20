#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "EspLink.h"

typedef enum
{
  BUTTON_PRESS,
  SET_FIELD,
  REFRESH,
  LOAD,
} WebServerCommand;

typedef void (*WebServerCallback)(WebServerCommand command, char * data, int dataLen);

typedef struct
{
  const char *  PROGMEM  url;
  WebServerCallback      callback;
} WebMethod;


typedef enum {
  WS_LOAD=0,
  WS_REFRESH,
  WS_BUTTON,
  WS_SUBMIT,
} RequestReason;

typedef enum
{
  WEB_STRING=0,
  WEB_NULL,
  WEB_INTEGER,
  WEB_BOOLEAN,
  WEB_FLOAT,
  WEB_JSON
} WebValueType;

class WebServer
{
  friend void webServerCallback(CmdRequest *req);
  
  private:
    const WebMethod * PROGMEM methods;
    Stream                   &stream;
    static WebServer *        instance;

    void                      invokeMethod(RequestReason reason, WebMethod * method, CmdRequest *req);
    void                      handleRequest(CmdRequest *req);

    uint8_t                   remote_ip[4];
    uint16_t                  remote_port;

    int16_t                   args_to_send;

    char *                    value_ptr;

    uint32_t                  last_connect_ts;
    
  protected:
    EspLink espLink;
    
  public:
    WebServer(Stream &stream, const WebMethod * PROGMEM methods);

    void init();
    void loop();

    void registerCallback();

    static WebServer * getInstance() { return instance; }
    uint8_t *          getRemoteIp() { return remote_ip; }
    uint16_t           getRemotePort() { return remote_port; }

    void               setArgNum(uint8_t num);
    void               setArgInt(const char * name, int32_t value);
    void               setArgJson(const char * name, const char * value);
    void               setArgString(const char * name, const char * value);
    void               setArgStringP(const char * name, const char * value);
    void               setArgBoolean(const char * name, uint8_t value);

    int32_t            getArgInt();
    char *             getArgString();
    uint8_t            getArgBoolean();
};

#endif /* WEB_SERVER_H */

