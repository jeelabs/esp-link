/*
 * tcp.h
 *
 *  Created on: Sep 2nd 2016
 *      Author: BeeGee
 */

#ifndef MODULES_TCP_H_
#define MODULES_TCP_H_

#include "cmd.h"

void TCP_Setup(CmdPacket *cmd);
void TCP_Send(CmdPacket *cmd);

#define SOCKET_CLIENT 0 // TCP socket client for sending only, doesn't wait for response from server
#define SOCKET_CLIENT_LISTEN 1 // TCP socket client, waits for response from server after sending
#define SOCKET_SERVER 2 // TCP socket server

#endif /* MODULES_TCP_H_ */
