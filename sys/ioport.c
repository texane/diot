/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 22:57:36 2009 texane
** Last update Thu Apr 16 13:45:28 2009 texane
*/



#include <wdm.h>
#include "../common/diotTypes.h"



/* internal types
 */

struct ioportRange
{
  /* todo: use USHORTs */

  ULONG_PTR firstPort;
  ULONG_PTR lastPort;
};


/* globals
 */

static __declspec(align(4)) volatile ULONG sharedRefCount = 1;
static ULONG ioportRangeCount = 0;
static struct ioportRange* ioportRanges;



/* locking routines. cannot use spinlock
   or a recursive locking path occurs.
   todo: make them truly atomic
 */

static BOOLEAN acquireShared(void)
{
  if (!sharedRefCount)
    return FALSE;

  ++sharedRefCount;

  return TRUE;
}


static void releaseShared(void)
{
  --sharedRefCount;
}


static void acquireExclusive(void)
{
  /* wait for the sharedRefCount to be 1
   */

 waitForExclusive:

  while (sharedRefCount > 1)
    ;

  if (!sharedRefCount)
    {
      /* lost the race */

      goto waitForExclusive;
    }

  sharedRefCount = 0;
}


static void releaseExclusive(void)
{
  sharedRefCount = 1;
}


/* exported
 */

NTSTATUS ioportInitialize(void)
{
  sharedRefCount = 1;

  return STATUS_SUCCESS;
}


void ioportCleanup(void)
{
  struct ioportRange* ranges;
  ULONG rangeCount;

  acquireExclusive();

  ranges = ioportRanges;
  rangeCount = ioportRangeCount;

  ioportRanges = NULL;
  ioportRangeCount = 0;

  releaseExclusive();

  if (ranges != NULL)
    ExFreePoolWithTag(ranges, DIOT_POOL_TAG);
}


NTSTATUS ioportSetRanges(const struct diotRange* PortRanges, ULONG RangeCount)
{
  const ULONG savedRangeCount = RangeCount;
  struct ioportRange* ranges;
  struct ioportRange* pos;

  ranges = ExAllocatePoolWithTag(NonPagedPool, RangeCount * sizeof(struct ioportRange), DIOT_POOL_TAG);
  if (ranges == NULL)
    return STATUS_INSUFFICIENT_RESOURCES;

  pos = ranges;

  while (RangeCount--)
    {
      pos->firstPort = (ULONG_PTR)PortRanges->firstByte.QuadPart;
      pos->lastPort = (ULONG_PTR)PortRanges->lastByte.QuadPart;

      ++PortRanges;
      ++pos;
    }

  acquireExclusive();

  pos = ioportRanges;

  ioportRangeCount = savedRangeCount;
  ioportRanges = ranges;

  releaseExclusive();

  if (pos != NULL)
    ExFreePoolWithTag(pos, DIOT_POOL_TAG);

  return STATUS_SUCCESS;
}



BOOLEAN ioportFind(ULONG_PTR Port)
{
  const struct ioportRange* pos;
  ULONG rangeIndex;
  BOOLEAN isFound = FALSE;

  if (acquireShared() == FALSE)
    return FALSE;

  pos = ioportRanges;

  for (rangeIndex = 0; rangeIndex < ioportRangeCount; ++rangeIndex)
    if (Port >= pos->firstPort && Port <= pos->lastPort)
      {
	isFound = TRUE;
	break;
      }

  releaseShared();

  return isFound;
}
