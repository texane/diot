/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 22:58:00 2009 texane
** Last update Wed Apr 15 22:58:02 2009 texane
*/



#include <wdm.h>
#include "mmio.h"
#include "ia32.h"
#include "../../common/diotTypes.h"
#include "../../common/diotDebug.h"



/* types
 */

struct mmioMapping
{
  struct mmioMapping* nextMapping;
  struct mmioMapping* prevMapping;
  ULONG_PTR virtualAddress;
  SIZE_T numberOfBytes;
};


struct mmioRange
{
  PHYSICAL_ADDRESS physicalAddress;
  SIZE_T numberOfBytes;
  struct mmioMapping* mappings;
};



/* globals
 */

static KSPIN_LOCK mmioRangeLock;
static ULONG mmioRangeCount = 0;
static struct mmioRange* mmioRanges = NULL;



/* internal
 */

static void destroyMmioRanges(struct mmioRange* Ranges, ULONG RangeCount)
{
  struct mmioRange* range = Ranges;
  struct mmioMapping* curMapping;
  struct mmioMapping* prevMapping;

  while (RangeCount--)
    {
      curMapping = range->mappings;

      while (curMapping != NULL)
	{
	  prevMapping = curMapping;
	  curMapping = curMapping->nextMapping;

	  /* not doing this may cause a pf on a valid
	     mapping that cannot be handled. since we
	     assume io space mappings are valid, mark
	     them as such to avoid the page fault.
	   */

	  ia32MarkPagesValid(prevMapping->virtualAddress, prevMapping->numberOfBytes);

	  ExFreePoolWithTag(prevMapping, DIOT_POOL_TAG);
	}

      ++range;
    }

  ExFreePoolWithTag(Ranges, DIOT_POOL_TAG);
}


static BOOLEAN intersectRange(const struct mmioRange* Range,
			      PHYSICAL_ADDRESS PhysicalAddress,
			      SIZE_T Size)
{
  if (PhysicalAddress.QuadPart < Range->physicalAddress.QuadPart)
    {
      if (PhysicalAddress.QuadPart + Size > Range->physicalAddress.QuadPart)
	return TRUE;
    }
  else
    {
      if (Range->physicalAddress.QuadPart + Range->numberOfBytes > PhysicalAddress.QuadPart)
	return TRUE;
    }

  return FALSE;
}


static BOOLEAN findRangeSafe(PHYSICAL_ADDRESS PhysicalAddress,
			     SIZE_T Size,
			     struct mmioRange** Range)
{
  /* assume mmioRangeLock */

  struct mmioRange* range = mmioRanges;
  ULONG rangeCount = mmioRangeCount;
  BOOLEAN isFound = FALSE;

  *Range = NULL;

  while (rangeCount--)
    {
      if (intersectRange(range, PhysicalAddress, Size) == TRUE)
	{
	  isFound = TRUE;
	  *Range = range;
	  break;
	}

      ++range;
    }

  return isFound;
}


static BOOLEAN intersectMapping(const struct mmioMapping* Mapping,
				ULONG_PTR VirtualAddress,
				SIZE_T Size)
{
  if (VirtualAddress < Mapping->virtualAddress)
    {
      if (VirtualAddress + Size > Mapping->virtualAddress)
	return TRUE;
    }
  else
    {
      if (Mapping->virtualAddress + Mapping->numberOfBytes > VirtualAddress)
	return TRUE;
    }

  return FALSE;
}


static BOOLEAN findMappingSafe(ULONG_PTR VirtualAddress,
			       SIZE_T Size,
			       struct mmioRange** Range,
			       struct mmioMapping** Mapping)
{
  /* assume mmioRangeLock */

  struct mmioRange* range = mmioRanges;
  ULONG rangeCount = mmioRangeCount;
  BOOLEAN isFound = FALSE;
  struct mmioMapping* mapping;

  *Range = NULL;
  *Mapping = NULL;

  while (rangeCount--)
    {
      for (mapping = range->mappings; mapping != NULL; mapping = mapping->nextMapping)
	if (intersectMapping(mapping, VirtualAddress, Size) == TRUE)
	  {
	    *Range = range;
	    *Mapping = mapping;

	    isFound = TRUE;

	    break;
	  }

      ++range;
    }

  return isFound;
}



/* exported
 */

NTSTATUS mmioInitialize(void)
{
  KeInitializeSpinLock(&mmioRangeLock);

  return STATUS_SUCCESS;
}


void mmioCleanup(void)
{
  struct mmioRange* ranges;
  ULONG rangeCount;
  KIRQL irql;

  KeAcquireSpinLock(&mmioRangeLock, &irql);

  ranges = mmioRanges;
  rangeCount = mmioRangeCount;

  mmioRanges = NULL;
  mmioRangeCount = 0;

  KeReleaseSpinLock(&mmioRangeLock, irql);

  if (rangeCount)
    destroyMmioRanges(ranges, rangeCount);
}


NTSTATUS mmioSetRanges(const struct diotRange* Ranges, ULONG RangeCount)
{
  struct mmioRange* oldRanges;
  struct mmioRange* mmioRange;
  struct mmioRange* ranges;
  const struct diotRange* diotRange;
  ULONG oldRangeCount;
  ULONG rangeIndex;
  KIRQL irql;

  ranges = ExAllocatePoolWithTag(NonPagedPool, RangeCount * sizeof(struct mmioRange), DIOT_POOL_TAG);
  if (ranges == NULL)
    return STATUS_INSUFFICIENT_RESOURCES;

  diotRange = Ranges;
  mmioRange = ranges;

  for (rangeIndex = 0; rangeIndex < RangeCount; ++rangeIndex)
    {
      mmioRange->physicalAddress.QuadPart = diotRange->firstByte.QuadPart;
      mmioRange->numberOfBytes = (SIZE_T)(diotRange->lastByte.QuadPart - diotRange->firstByte.QuadPart + 1);
      mmioRange->mappings = NULL;

      ++mmioRange;
      ++diotRange;
    }

  KeAcquireSpinLock(&mmioRangeLock, &irql);

  oldRangeCount = mmioRangeCount;
  oldRanges = mmioRanges;

  mmioRanges = ranges;
  mmioRangeCount = RangeCount;

  KeReleaseSpinLock(&mmioRangeLock, irql);

  if (oldRangeCount)
    destroyMmioRanges(oldRanges, oldRangeCount);

  return STATUS_SUCCESS;
}


BOOLEAN mmioFindMapping(ULONG_PTR VirtualAddress,
			SIZE_T Size,
			PHYSICAL_ADDRESS* MappingPhysicalAddress,
			ULONG_PTR* MappingVirtualAdress,
			SIZE_T* MappingNumberOfBytes)
{
  struct mmioRange* range = NULL;
  struct mmioMapping* mapping = NULL;
  BOOLEAN isFound = FALSE;
  KIRQL irql;

  KeAcquireSpinLock(&mmioRangeLock, &irql);

  isFound = findMappingSafe(VirtualAddress, Size, &range, &mapping);

  if (isFound == TRUE)
    {
      *MappingPhysicalAddress = range->physicalAddress;
      *MappingVirtualAdress = mapping->virtualAddress;
      *MappingNumberOfBytes = mapping->numberOfBytes;
    }

  KeReleaseSpinLock(&mmioRangeLock, irql);

  return isFound;
}


BOOLEAN mmioNotifyMappingCreation(ULONG_PTR VirtualAddress,
				  PHYSICAL_ADDRESS PhysicalAddress,
				  SIZE_T Size)
{
  BOOLEAN isSuccess = FALSE;
  struct mmioRange* range;
  struct mmioMapping* mapping;
  KIRQL irql;

  KeAcquireSpinLock(&mmioRangeLock, &irql);

  if (findRangeSafe(PhysicalAddress, Size, &range) == FALSE)
    goto onError;

  mapping = ExAllocatePoolWithTag(NonPagedPool, sizeof(struct mmioMapping), DIOT_POOL_TAG);
  if (mapping == NULL)
    goto onError;

  mapping->virtualAddress = VirtualAddress;
  mapping->numberOfBytes = Size;

  mapping->prevMapping = NULL;

  mapping->nextMapping = range->mappings;

  if (range->mappings != NULL)
    range->mappings->prevMapping = mapping;

  range->mappings = mapping;

  /* success */

  isSuccess = TRUE;

 onError:

  KeReleaseSpinLock(&mmioRangeLock, irql);

  return isSuccess;
}


BOOLEAN mmioNotifyMappingDeletion(ULONG_PTR VirtualAddress, SIZE_T Size)
{
  BOOLEAN isSuccess = FALSE;
  struct mmioMapping* mapping = NULL;
  struct mmioRange* range;
  KIRQL irql;

  KeAcquireSpinLock(&mmioRangeLock, &irql);

  isSuccess = findMappingSafe(VirtualAddress, Size, &range, &mapping);
  if (isSuccess == FALSE)
    goto onError;

  if (mapping == range->mappings)
    {
      range->mappings->prevMapping = NULL;
      range->mappings = mapping->nextMapping;
    }
  else
    {
      mapping->prevMapping->nextMapping = mapping->nextMapping;
    }

  if (mapping->nextMapping != NULL)
    {
      mapping->nextMapping->prevMapping = mapping->prevMapping;
    }

  /* success */

  isSuccess = TRUE;

 onError:

  KeReleaseSpinLock(&mmioRangeLock, irql);

  if (mapping != NULL)
    ExFreePoolWithTag(mapping, DIOT_POOL_TAG);

  return isSuccess;
}
