/*
 * api.h
 *
 *  Created on: Mar 4, 2015
 *      Author: Minh
 */

#ifndef MODULES_API_H_
#define MODULES_API_H_

#include "cmd.h"

void REST_Setup(CmdPacket *cmd);
void REST_Request(CmdPacket *cmd);
void REST_SetHeader(CmdPacket *cmd);

#endif /* MODULES_INCLUDE_API_H_ */
