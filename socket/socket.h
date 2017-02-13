/*
 * socket.h
 *
 *  Created on: Sep 16th 2016
 *      Author: BeeGee
 */

#ifndef MODULES_SOCKET_H_
#define MODULES_SOCKET_H_

#include "cmd.h"

void SOCKET_Setup(CmdPacket *cmd);
void SOCKET_Send(CmdPacket *cmd);

// Socket mode
typedef enum {
  SOCKET_TCP_CLIENT = 0, /**< TCP socket client for sending only, doesn't wait for response from server */
  SOCKET_TCP_CLIENT_LISTEN, /**< TCP socket client, waits for response from server after sending */
  SOCKET_TCP_SERVER, /**< TCP socket server */
  SOCKET_UDP,  /**< UDP socket for sending and receiving UDP packets */
} socketMode;

// Callback type
typedef enum {
  USERCB_SENT = 0, /**< Data send finished */
  USERCB_RECV, /**< Data received */
  USERCB_RECO, /**< Connection error */
  USERCB_CONN, /**< Connection event */
} cbType;

// Connection status
typedef enum {
  CONNSTAT_DIS = 0, // Disconnected
  CONNSTAT_CON, // Connected
} connStat;

#endif /* MODULES_SOCKET_H_ */
