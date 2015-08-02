#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__

bool tcpClientCommand(char cmd, char *cmdBuf);
void tcpClientSendChar(uint8_t chan, char c);
void tcpClientSendPush(uint8_t chan);

#endif /* __TCP_CLIENT_H__ */
