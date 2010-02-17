/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 22:57:50 2009 texane
** Last update Wed Apr 15 22:57:52 2009 texane
*/



#ifndef IOPORT_H_INCLUDED
# define IOPORT_H_INCLUDED



#include <wdm.h>
#include "../common/diotTypes.h"



NTSTATUS ioportInitialize(void);
void ioportCleanup(void);
NTSTATUS ioportSetRanges(const struct diotRange*, ULONG);
BOOLEAN ioportFind(ULONG_PTR);



#endif /* ! IOPORT_H_INCLUDED */
