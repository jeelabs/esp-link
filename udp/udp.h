/*
 * udp.h
 *
 *  Created on: Sep 2nd 2016
 *      Author: BeeGee
 */

#ifndef MODULES_UDP_H_
#define MODULES_UDP_H_

#include "cmd.h"

void UDP_Setup(CmdPacket *cmd);
void UDP_Send(CmdPacket *cmd);

#endif /* MODULES_UDP_H_ */
