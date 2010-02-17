/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 22:58:05 2009 texane
** Last update Wed Apr 15 22:58:07 2009 texane
*/



#ifndef MMIO_H_INCLUDED
# define MMIO_H_INCLUDED



#include <wdm.h>
#include "../common/diotTypes.h"



NTSTATUS mmioInitialize(void);
void mmioCleanup(void);
NTSTATUS mmioSetRanges(const struct diotRange*, ULONG);
BOOLEAN mmioFindMapping(ULONG_PTR, SIZE_T, PHYSICAL_ADDRESS*, ULONG_PTR*, SIZE_T*);
BOOLEAN mmioNotifyMappingCreation(ULONG_PTR, PHYSICAL_ADDRESS, SIZE_T);
BOOLEAN mmioNotifyMappingDeletion(ULONG_PTR, SIZE_T);



#endif /* ! MMIO_H_INCLUDED */
