#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__

// max number of channels the client can open
#define MAX_TCP_CHAN 8

// Parse and perform the commandm cmdBuf must be null-terminated
bool tcpClientCommand(uint8_t chan, char cmd, char *cmdBuf);

// Append a character to the specified channel
void tcpClientSendChar(uint8_t chan, char c);

// Enqueue the buffered characters for transmission on the specified channel
void tcpClientSendPush(uint8_t chan);

#endif /* __TCP_CLIENT_H__ */
