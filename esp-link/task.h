/*
 * task.h
 *
  * Copyright 2015 Susi's Strolch
 *
 * For license information see projects "License.txt"
 *
 *
 */

#ifndef	USRTASK_H
#define USRTASK_H

#define _taskPrio        1
#define _task_queueLen  64

uint8_t register_usr_task (os_task_t event);
bool	post_usr_task(uint8_t task, os_param_t par);

#endif
