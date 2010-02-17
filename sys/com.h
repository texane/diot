/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 22:56:34 2009 texane
** Last update Wed Apr 15 22:56:36 2009 texane
*/



#ifndef COM_H_INCLUDED
# define COM_H_INCLUDED



#include <wdm.h>
#include "../common/diotTypes.h"



NTSTATUS comInitialize(PDRIVER_OBJECT);
void comCleanup(void);

struct diotEvent* comAllocDiotEvent(enum diotEventType, SIZE_T);
void comFreeDiotEvent(struct diotEvent*);
void comPushDiotEvent(struct diotEvent*);



#endif /* ! COM_H_INCLUDED */
