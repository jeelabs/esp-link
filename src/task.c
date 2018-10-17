/*
 * task.c
 *
 * Copyright 2015 Susi's Strolch
 *
 * For license information see projects "License.txt"
 *
 * Not sure if it's save to use ICACHE_FLASH_ATTR, so we're running from RAM
 */

#undef USRTASK_DBG

#include "esp8266.h"
#include <task.h>

#define MAXUSRTASKS	 8

#ifdef USRTASK_DBG
#define DBG_USRTASK(format, ...) os_printf(format, ## __VA_ARGS__)
#else
#define DBG_USRTASK(format, ...) do { } while(0)
#endif

LOCAL os_event_t *_task_queue   = NULL;		// system_os_task queue
LOCAL os_task_t  *usr_task_queue = NULL;	// user task queue

// it seems save to run the usr_event_handler from RAM, so no ICACHE_FLASH_ATTR here...

LOCAL void usr_event_handler(os_event_t *e)
{
  DBG_USRTASK("usr_event_handler: event %p (sig=%d, par=%p)\n", e, (int)e->sig, (void *)e->par);
  if (usr_task_queue[e->sig] == NULL || e->sig < 0 || e->sig >= MAXUSRTASKS) {
    os_printf("usr_event_handler: task %d %s\n", (int)e->sig,
	       usr_task_queue[e->sig] == NULL ? "not registered" : "out of range");
    return;
  }
  (usr_task_queue[e->sig])(e);
}

LOCAL void init_usr_task() {
  if (_task_queue == NULL)
    _task_queue = (os_event_t *)os_zalloc(sizeof(os_event_t) * _task_queueLen);

  if (usr_task_queue == NULL)
    usr_task_queue = (os_task_t *)os_zalloc(sizeof(os_task_t) * MAXUSRTASKS);

  system_os_task(usr_event_handler, _taskPrio, _task_queue, _task_queueLen);
}

// public functions
bool post_usr_task(uint8_t task, os_param_t par)
{
  return system_os_post(_taskPrio, task, par);
}

uint8_t register_usr_task (os_task_t event)
{
  int task;

  DBG_USRTASK("register_usr_task: %p\n", event);
  if (_task_queue   == NULL || usr_task_queue == NULL)
    init_usr_task();

  for (task = 0; task < MAXUSRTASKS; task++) {
    if (usr_task_queue[task] == event)
      return task;		// task already registered - bail out...
  }

  for (task = 0; task < MAXUSRTASKS; task++) {
    if (usr_task_queue[task] == NULL) {
      DBG_USRTASK("register_usr_task: assign task #%d\n", task);
      usr_task_queue[task] = event;
      break;
    }
  }
  return task;
}

