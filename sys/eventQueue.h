/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 22:57:01 2009 texane
** Last update Wed Apr 15 22:57:02 2009 texane
*/



#ifndef EVENT_QUEUE_H_INCLUDED
# define EVENT_QUEUE_H_INCLUDED



#include <wdm.h>
#include "../common/diotTypes.h"



NTSTATUS eventQueueInitialize(void);
void eventQueueCleanup(void);
BOOLEAN eventQueueSetUserEvent(PVOID);
PVOID eventQueueGetUserEvent(void);
NTSTATUS eventQueueAlloc(struct diotEvent**, enum diotEventType, SIZE_T);
void eventQueueFree(struct diotEvent*);
void eventQueuePush(struct diotEvent*);
void eventQueuePop(PUCHAR, SIZE_T, SIZE_T*);

#if 0 /* unit */
void eventQueueStartUnit(void);
void eventQueueStopUnit(void);
#endif /* unit */



#endif /* ! EVENT_QUEUE_H_INCLUDED */
