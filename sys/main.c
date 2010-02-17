/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 22:57:55 2009 texane
** Last update Mon Apr 20 09:39:11 2009 texane
*/



#include <ntddk.h>
#include <wdm.h>
#include "mmio.h"
#include "ioport.h"
#include "com.h"
#include "hook.h"
#include "disasm.h"
#include "safeboot.h"
#include "ia32.h"
#include "eventQueue.h"
#include "../../common/diotTypes.h"
#include "../../common/diotDebug.h"



static void printDiotInsn(const struct diotInsn* Insn)
{
  const char* srcType;
  const char* dstType;

  const char* sizeString;

  switch (Insn->insnType)
    {
    case DIOT_INSN_TYPE_MEM_TO_REG:
      srcType = "m";
      dstType = "r";
      break;

    case DIOT_INSN_TYPE_IMM_TO_MEM:
      srcType = "i";
      goto toMemCommonCase;

    case DIOT_INSN_TYPE_REG_TO_MEM:
      srcType = "r";
      goto toMemCommonCase;

    case DIOT_INSN_TYPE_MEM_TO_MEM:
      srcType = "m";

    toMemCommonCase:
      dstType = "m";
      break;

    default:
      srcType = "?";
      dstType = "?";
      break;
    }

  switch (Insn->operandSize)
    {
    case 8:
      sizeString = "b";
      break;

    case 16:
      sizeString = "w";
      break;

    case 32:
      sizeString = "d";
      break;

    case 64:
      sizeString = "q";
      break;

    default:
      sizeString = "?";
      break;
    }

  DbgPrint("@0x%08x (%s) %s[0x%08x] <- %s[0x%08x] == 0x%08x\n",
	   Insn->insnAddress,
	   sizeString,
	   dstType,
	   Insn->dstAddress,
	   srcType,
	   Insn->srcAddress,
	   Insn->operandValue);
}



/* push an ioport to user
 */

static void pushIoportEvent(BOOLEAN isInput, SIZE_T Count, UCHAR Size, ULONG_PTR Port, ULONG_PTR Value)
{
  struct diotEvent* event;

  if (eventQueueAlloc(&event, DIOT_EVENT_IOPORT, 0) != STATUS_SUCCESS)
    return ;

  event->data.ioportEvent.isInput = isInput;
  event->data.ioportEvent.count = Count;
  event->data.ioportEvent.size = Size;
  event->data.ioportEvent.port = Port;
  event->data.ioportEvent.value = Value;

  eventQueuePush(event);
}



/* {READ,WRITE}_PORT{_BUFFER}_{UCHAR,ULONG,USHORT} hooks
 */

#define MAKE_READ_PORT_HOOK(TYPE)						\
										\
static PHOOK_CONTEXT READ_PORT_ ## TYPE ## _HookContext = NULL;			\
										\
static TYPE __stdcall _READ_PORT_ ## TYPE(ULONG_PTR OriginalStackPointer)	\
{										\
  TYPE* port;									\
  TYPE value;									\
  HookRefFunction(READ_PORT_ ## TYPE ## _HookContext);				\
  port = *(TYPE**)OriginalStackPointer;						\
  value =(TYPE)HookCallFunction(OriginalStackPointer, READ_PORT_ ## TYPE ## _HookContext); \
  if (ioportFind((ULONG_PTR)port) == FALSE)					\
    goto onError;								\
  pushIoportEvent(TRUE, 1, sizeof(TYPE) * 8, (ULONG_PTR)port, (ULONG_PTR)value);\
 onError:									\
  HookUnrefFunction(READ_PORT_ ## TYPE ## _HookContext);			\
  return value;									\
}


#define MAKE_READ_PORT_BUFFER_HOOK(TYPE)					\
										\
static PHOOK_CONTEXT READ_PORT_BUFFER_ ## TYPE ## _HookContext = NULL;		\
										\
static VOID __stdcall _READ_PORT_BUFFER_ ## TYPE(ULONG_PTR OriginalStackPointer)\
{										\
  TYPE* port;									\
  const TYPE* buffer;								\
  ULONG count;									\
  HookRefFunction(READ_PORT_BUFFER_ ## TYPE ## _HookContext);			\
  port = (TYPE*)*((ULONG_PTR*)OriginalStackPointer + 0);			\
  buffer = (const TYPE*)((ULONG_PTR*)OriginalStackPointer + 1);			\
  count = (ULONG)((ULONG_PTR*)OriginalStackPointer + 2);			\
  HookCallFunction(OriginalStackPointer, READ_PORT_BUFFER_ ## TYPE ## _HookContext); \
  if (ioportFind((ULONG_PTR)port) == FALSE)					\
    goto onError;								\
  pushIoportEvent(TRUE, count, sizeof(TYPE) * 8, (ULONG_PTR)port, (ULONG_PTR)buffer); \
 onError:									\
  HookUnrefFunction(READ_PORT_BUFFER_ ## TYPE ## _HookContext);			\
}


#define MAKE_WRITE_PORT_HOOK(TYPE)						\
										\
static PHOOK_CONTEXT WRITE_PORT_ ## TYPE ## _HookContext = NULL;		\
										\
static VOID __stdcall _WRITE_PORT_ ## TYPE(ULONG_PTR OriginalStackPointer)	\
{										\
  ULONG_PTR port;								\
  TYPE value;									\
  HookRefFunction(WRITE_PORT_ ## TYPE ## _HookContext);				\
  port = *((const ULONG_PTR*)OriginalStackPointer + 0);				\
  value = (TYPE)*((const ULONG_PTR*)OriginalStackPointer + 1);			\
  HookCallFunction(OriginalStackPointer, WRITE_PORT_ ## TYPE ## _HookContext);	\
  if (ioportFind((ULONG_PTR)port) == FALSE)					\
    goto onError;								\
  pushIoportEvent(FALSE, 1, sizeof(TYPE) * 8, (ULONG_PTR)port, (ULONG_PTR)value); \
 onError:									\
  HookUnrefFunction(WRITE_PORT_ ## TYPE ## _HookContext);			\
}


#define MAKE_WRITE_PORT_BUFFER_HOOK(TYPE)					\
										\
static PHOOK_CONTEXT WRITE_PORT_BUFFER_ ## TYPE ## _HookContext = NULL;		\
										\
static VOID __stdcall _WRITE_PORT_BUFFER_ ## TYPE(ULONG_PTR OriginalStackPointer)\
{										\
  TYPE* port;									\
  const TYPE* buffer;								\
  ULONG count;									\
  HookRefFunction(WRITE_PORT_BUFFER_ ## TYPE ## _HookContext);			\
  port = (TYPE*)*((ULONG_PTR*)OriginalStackPointer + 0);			\
  buffer = (const TYPE*)((ULONG_PTR*)OriginalStackPointer + 1);			\
  count = (ULONG)((ULONG_PTR*)OriginalStackPointer + 2);			\
  HookCallFunction(OriginalStackPointer, WRITE_PORT_BUFFER_ ## TYPE ## _HookContext); \
  if (ioportFind((ULONG_PTR)port) == FALSE)					\
    goto onError;								\
  pushIoportEvent(FALSE, count, sizeof(TYPE) * 8, (ULONG_PTR)port, (ULONG_PTR)buffer); \
 onError:									\
  HookUnrefFunction(WRITE_PORT_BUFFER_ ## TYPE ## _HookContext);		\
}


MAKE_READ_PORT_HOOK(UCHAR);
MAKE_READ_PORT_HOOK(USHORT);
MAKE_READ_PORT_HOOK(ULONG);

MAKE_READ_PORT_BUFFER_HOOK(UCHAR);
MAKE_READ_PORT_BUFFER_HOOK(USHORT);
MAKE_READ_PORT_BUFFER_HOOK(ULONG);

MAKE_WRITE_PORT_HOOK(UCHAR);
MAKE_WRITE_PORT_HOOK(USHORT);
MAKE_WRITE_PORT_HOOK(ULONG);

MAKE_WRITE_PORT_BUFFER_HOOK(UCHAR);
MAKE_WRITE_PORT_BUFFER_HOOK(USHORT);
MAKE_WRITE_PORT_BUFFER_HOOK(ULONG);


static NTSTATUS installPortHooks(void)
{
  /* read */

  READ_PORT_UCHAR_HookContext =
    HookSetFunction((ULONG_PTR)READ_PORT_UCHAR, (ULONG_PTR)_READ_PORT_UCHAR, 0x04);
  if (READ_PORT_UCHAR_HookContext == NULL)
    goto onError;

  READ_PORT_USHORT_HookContext =
    HookSetFunction((ULONG_PTR)READ_PORT_USHORT, (ULONG_PTR)_READ_PORT_USHORT, 0x04);
  if (READ_PORT_USHORT_HookContext == NULL)
    goto onError;

  READ_PORT_ULONG_HookContext =
    HookSetFunction((ULONG_PTR)READ_PORT_ULONG, (ULONG_PTR)_READ_PORT_ULONG, 0x04);
  if (READ_PORT_ULONG_HookContext == NULL)
    goto onError;

  /* read buffer */

  READ_PORT_BUFFER_UCHAR_HookContext =
    HookSetFunction((ULONG_PTR)READ_PORT_BUFFER_UCHAR, (ULONG_PTR)_READ_PORT_BUFFER_UCHAR, 0x0c);
  if (READ_PORT_BUFFER_UCHAR_HookContext == NULL)
    goto onError;

  READ_PORT_BUFFER_USHORT_HookContext =
    HookSetFunction((ULONG_PTR)READ_PORT_BUFFER_USHORT, (ULONG_PTR)_READ_PORT_BUFFER_USHORT, 0x0c);
  if (READ_PORT_BUFFER_USHORT_HookContext == NULL)
    goto onError;

  READ_PORT_BUFFER_ULONG_HookContext =
    HookSetFunction((ULONG_PTR)READ_PORT_BUFFER_ULONG, (ULONG_PTR)_READ_PORT_BUFFER_ULONG, 0x0c);
  if (READ_PORT_BUFFER_ULONG_HookContext == NULL)
    goto onError;

  /* write */

  WRITE_PORT_UCHAR_HookContext =
    HookSetFunction((ULONG_PTR)WRITE_PORT_UCHAR, (ULONG_PTR)_WRITE_PORT_UCHAR, 0x08);
  if (WRITE_PORT_UCHAR_HookContext == NULL)
    goto onError;

  WRITE_PORT_USHORT_HookContext =
    HookSetFunction((ULONG_PTR)WRITE_PORT_USHORT, (ULONG_PTR)_WRITE_PORT_USHORT, 0x08);
  if (WRITE_PORT_USHORT_HookContext == NULL)
    goto onError;

  WRITE_PORT_ULONG_HookContext =
    HookSetFunction((ULONG_PTR)WRITE_PORT_ULONG, (ULONG_PTR)_WRITE_PORT_ULONG, 0x08);
  if (WRITE_PORT_ULONG_HookContext == NULL)
    goto onError;

  /* write buffer */

  WRITE_PORT_BUFFER_UCHAR_HookContext =
    HookSetFunction((ULONG_PTR)WRITE_PORT_BUFFER_UCHAR, (ULONG_PTR)_WRITE_PORT_BUFFER_UCHAR, 0x0c);
  if (WRITE_PORT_BUFFER_UCHAR_HookContext == NULL)
    goto onError;

  WRITE_PORT_BUFFER_USHORT_HookContext =
    HookSetFunction((ULONG_PTR)WRITE_PORT_BUFFER_USHORT, (ULONG_PTR)_WRITE_PORT_BUFFER_USHORT, 0x0c);
  if (WRITE_PORT_BUFFER_USHORT_HookContext == NULL)
    goto onError;

  WRITE_PORT_BUFFER_ULONG_HookContext =
    HookSetFunction((ULONG_PTR)WRITE_PORT_BUFFER_ULONG, (ULONG_PTR)_WRITE_PORT_BUFFER_ULONG, 0x0c);
  if (WRITE_PORT_BUFFER_ULONG_HookContext == NULL)
    goto onError;

  return STATUS_SUCCESS;

 onError:

  if (READ_PORT_UCHAR_HookContext != NULL)
    {
      HookUnsetFunction(READ_PORT_UCHAR_HookContext);
      READ_PORT_UCHAR_HookContext = NULL;
    }

  if (READ_PORT_USHORT_HookContext != NULL)
    {
      HookUnsetFunction(READ_PORT_USHORT_HookContext);
      READ_PORT_USHORT_HookContext = NULL;
    }

  if (READ_PORT_ULONG_HookContext != NULL)
    {
      HookUnsetFunction(READ_PORT_ULONG_HookContext);
      READ_PORT_ULONG_HookContext = NULL;
    }

  if (READ_PORT_BUFFER_UCHAR_HookContext != NULL)
    {
      HookUnsetFunction(READ_PORT_BUFFER_UCHAR_HookContext);
      READ_PORT_BUFFER_UCHAR_HookContext = NULL;
    }

  if (READ_PORT_BUFFER_USHORT_HookContext != NULL)
    {
      HookUnsetFunction(READ_PORT_BUFFER_USHORT_HookContext);
      READ_PORT_BUFFER_USHORT_HookContext = NULL;
    }

  if (READ_PORT_BUFFER_ULONG_HookContext != NULL)
    {
      HookUnsetFunction(READ_PORT_BUFFER_ULONG_HookContext);
      READ_PORT_BUFFER_ULONG_HookContext = NULL;
    }

  if (WRITE_PORT_UCHAR_HookContext != NULL)
    {
      HookUnsetFunction(WRITE_PORT_UCHAR_HookContext);
      WRITE_PORT_UCHAR_HookContext = NULL;
    }

  if (WRITE_PORT_USHORT_HookContext != NULL)
    {
      HookUnsetFunction(WRITE_PORT_USHORT_HookContext);
      WRITE_PORT_USHORT_HookContext = NULL;
    }

  if (WRITE_PORT_ULONG_HookContext != NULL)
    {
      HookUnsetFunction(WRITE_PORT_ULONG_HookContext);
      WRITE_PORT_ULONG_HookContext = NULL;
    }

  if (WRITE_PORT_BUFFER_UCHAR_HookContext != NULL)
    {
      HookUnsetFunction(WRITE_PORT_BUFFER_UCHAR_HookContext);
      WRITE_PORT_BUFFER_UCHAR_HookContext = NULL;
    }

  if (WRITE_PORT_BUFFER_USHORT_HookContext != NULL)
    {
      HookUnsetFunction(WRITE_PORT_BUFFER_USHORT_HookContext);
      WRITE_PORT_BUFFER_USHORT_HookContext = NULL;
    }

  if (WRITE_PORT_BUFFER_ULONG_HookContext != NULL)
    {
      HookUnsetFunction(WRITE_PORT_BUFFER_ULONG_HookContext);
      WRITE_PORT_BUFFER_ULONG_HookContext = NULL;
    }

  return STATUS_UNSUCCESSFUL;
}



/* Mm{Un}MapIoSpace hooks
 */

static BOOLEAN areIntHooksInstalled(void);
static NTSTATUS installIntHooks(void);


static PHOOK_CONTEXT MmMapIoSpaceHookContext = NULL;
static PHOOK_CONTEXT MmUnmapIoSpaceHookContext = NULL;


static ULONG_PTR __stdcall _MmMapIoSpace(ULONG_PTR OriginalStackPointer)
{
  ULONG_PTR virtualAddress = (ULONG_PTR)NULL;
  PHYSICAL_ADDRESS physicalAddress;
  SIZE_T numberOfBytes;
  struct diotEvent* event;

  HookRefFunction(MmMapIoSpaceHookContext);

  physicalAddress = *(PPHYSICAL_ADDRESS)OriginalStackPointer;
  numberOfBytes = *(PSIZE_T)(OriginalStackPointer + sizeof(PHYSICAL_ADDRESS));

  virtualAddress = HookCallFunction(OriginalStackPointer, MmMapIoSpaceHookContext);

  if (virtualAddress == (ULONG_PTR)NULL)
    goto onError;

  if (mmioNotifyMappingCreation(virtualAddress, physicalAddress, numberOfBytes) == FALSE)
    {
      /* not intersted in the mapping */

      goto onError;
    }

  if (areIntHooksInstalled() == FALSE)
    {
      /* int hooks not yet installed */

      if (installIntHooks() != STATUS_SUCCESS)
	{
	  DIOT_DEBUG_ERROR("installIntHooks() == FALSE\n");
	  goto onError;
	}
    }

  /* mark pages as invalid */

  ia32MarkPagesInvalid(virtualAddress, numberOfBytes);

  /* notify user */

  if (eventQueueAlloc(&event, DIOT_EVENT_MMIO_MAPPING, 0) == STATUS_SUCCESS)
    {
      event->data.mappingEvent.isCreated = TRUE;
      event->data.mappingEvent.mapping.physicalAddress.QuadPart = physicalAddress.QuadPart;
      event->data.mappingEvent.mapping.virtualAddress = virtualAddress;
      event->data.mappingEvent.mapping.numberOfBytes = numberOfBytes;
      eventQueuePush(event);
    }

 onError:

  HookUnrefFunction(MmMapIoSpaceHookContext);

  return virtualAddress;
}


static VOID __stdcall _MmUnmapIoSpace(ULONG_PTR OriginalStackPointer)
{
  PVOID virtualAddress;
  SIZE_T numberOfBytes;
  struct diotEvent* event;

  HookRefFunction(MmUnmapIoSpaceHookContext);

  virtualAddress = *(PVOID*)OriginalStackPointer;
  numberOfBytes = *(PSIZE_T)(OriginalStackPointer + sizeof(PVOID));

  HookCallFunction(OriginalStackPointer, MmUnmapIoSpaceHookContext);

  if (mmioNotifyMappingDeletion((ULONG_PTR)virtualAddress, numberOfBytes) == TRUE)
    {
      /* notify user */

      if (eventQueueAlloc(&event, DIOT_EVENT_MMIO_MAPPING, 0) == STATUS_SUCCESS)
	{
	  event->data.mappingEvent.isCreated = FALSE;
	  event->data.mappingEvent.mapping.physicalAddress.QuadPart = 0ULL;
	  event->data.mappingEvent.mapping.virtualAddress = (ULONG_PTR)virtualAddress;
	  event->data.mappingEvent.mapping.numberOfBytes = numberOfBytes;
	  eventQueuePush(event);
	}
    }

  HookUnrefFunction(MmUnmapIoSpaceHookContext);
}



/* read a sized value at specified address
 */

ULONG_PTR readValueAt(ULONG_PTR VirtualAddress, SIZE_T Size)
{
  ULONG_PTR value;

#define GEN_SIZE_CASE(T, A, N) case sizeof(T): N = *(const T*)(A) ; break

  switch (Size)
    {
      GEN_SIZE_CASE(UCHAR, VirtualAddress, value);
      GEN_SIZE_CASE(USHORT, VirtualAddress, value);
      GEN_SIZE_CASE(ULONG, VirtualAddress, value);

    default:
      value = 0;
      break;
    }

  return value;
}



/* per cpu fault contexts
 */

#define MAX_CPU_COUNT 32


struct faultContext
{
#define INVALID_CPU_NUMBER ((ULONG)-1)
  __declspec(align(4)) volatile ULONG cpuNumber;
  struct disasmContext disasmContext;
  BOOLEAN isSingleStep;
  ULONG_PTR cr2;
  struct diotMmioInsnEvent event;
};


static struct faultContext faultContexts[MAX_CPU_COUNT];


static void initializeFaultContexts(void)
{
  struct faultContext* context = faultContexts;
  ULONG i;

  for (i = 0; i < MAX_CPU_COUNT; ++i)
    {
      context->cpuNumber = INVALID_CPU_NUMBER;
      ++context;
    }
}


static struct faultContext* getFaultContext(void)
{
  /* get a per cpu fault context
   */

  const ULONG cpuNumber = KeGetCurrentProcessorNumber();
  struct faultContext* context = faultContexts;
  ULONG oldCpuNumber;
  ULONG i;

  for (i = 0; i < MAX_CPU_COUNT; ++i)
    {
      if (context->cpuNumber == cpuNumber)
	return context;

      if (context->cpuNumber == INVALID_CPU_NUMBER)
	break;

      ++context;
    }

  /* not found, find a free context. */

  for (; i < MAX_CPU_COUNT; ++i)
    {
      oldCpuNumber = (ULONG)InterlockedCompareExchange((PVOID)&context->cpuNumber, cpuNumber, INVALID_CPU_NUMBER);

      if (oldCpuNumber == INVALID_CPU_NUMBER)
	{
	  /* found a free slot */

	  break;
	}

      ++context;
    }

  if (i == MAX_CPU_COUNT)
    {
      /* not found */

      return NULL;
    }

  /* initialize context */

  disasmInitialize(&context->disasmContext);

  return context;
}



/* KiTrap01 (debug exception)
 */

static PHOOK_CONTEXT KiTrap01HookContext = NULL;


static int __stdcall _KiTrap01(PULONG SavedRegisters, PULONG StackPointer)
{
  /* return > 0 if handled since no error
     code has to be removed on iret.
   */

  struct faultContext* faultContext;
  struct diotEvent* event;
  struct diotInsn* insn;
  int status = -1;

  HookRefInterruptGate(KiTrap01HookContext);

  if (!(ia32ReadDr6() & (1 << 14)))
    {
      /* not a single step fault */

      goto onError;
    }

  faultContext = getFaultContext();
  if (faultContext == NULL)
    {
      /* not previously set */

      goto onError;
    }

  /* read value now */

  insn = &faultContext->event.insn;

  if (insn->insnType == DIOT_INSN_TYPE_MEM_TO_REG)
    {
      switch (insn->operandSize)
	{
	case 8:
	  /* todo: what if high part */
	  insn->operandValue = (UCHAR)SavedRegisters[(SIZE_T)faultContext->event.insn.dstAddress];
	  break;

	case 16:
	  insn->operandValue = (USHORT)SavedRegisters[(SIZE_T)faultContext->event.insn.dstAddress];
	  break;

	case 32:
	  insn->operandValue = (ULONG)SavedRegisters[(SIZE_T)faultContext->event.insn.dstAddress];
	  break;

	default:
	  insn->operandValue = 0;
	  break;
	}
    }
  else if (insn->insnType == DIOT_INSN_TYPE_MEM_TO_MEM)
    {
      /* see trap0e comment */

      if (insn->dstAddress != faultContext->cr2)
	insn->operandValue = readValueAt(insn->dstAddress, insn->operandSize / 8);
    }

  if (eventQueueAlloc(&event, DIOT_EVENT_MMIO_INSN, 0) == STATUS_SUCCESS)
    {
      memcpy(&event->data.insnEvent.mapping, &faultContext->event.mapping, sizeof(struct diotMapping));
      memcpy(&event->data.insnEvent.insn, &faultContext->event.insn, sizeof(struct diotInsn));
      eventQueuePush(event);
    }

  if (faultContext->isSingleStep == TRUE)
    {
      /* we turned the cpu single step mode on */

      ia32MarkPageInvalid(faultContext->cr2);
      StackPointer[0x02] &= ~(1 << 8);
    }

  /* handled without error code */

  status = 1;

  /* always clear the context */

  faultContext->isSingleStep = FALSE;
  faultContext->cr2 = (ULONG_PTR)NULL;

 onError:

  HookUnrefInterruptGate(KiTrap01HookContext);

  return status;
}



/* KiTrap0e (page fault exception)
 */

static PHOOK_CONTEXT KiTrap0eHookContext = NULL;


static int __stdcall _KiTrap0e(PULONG SavedRegisters, PULONG StackPointer)
{
  /* return 0 if the fault is handled
   */

  int status = -1;
  BOOLEAN isFound;
  ULONG_PTR virtualAddress;
  ULONG_PTR mappingVirtualAddress;
  PHYSICAL_ADDRESS mappingPhysicalAddress;
  SIZE_T mappingNumberOfBytes;
  struct faultContext* faultContext;
  struct diotInsn* insn;
  SIZE_T insnSize;

  HookRefInterruptGate(KiTrap0eHookContext);

  if (StackPointer[0x00] & 1)
    {
      /* page level prot violation */

      goto onError;
    }

  virtualAddress = ia32ReadCr2();

  isFound =
    mmioFindMapping(virtualAddress,
		    1,
		    &mappingPhysicalAddress,
		    &mappingVirtualAddress,
		    &mappingNumberOfBytes);

  if (isFound == FALSE)
    {
      /* not traced */

      goto onError;
    }

  /* reset fault context */

  faultContext = getFaultContext();
  if (faultContext == NULL)
    goto onError;

  faultContext->isSingleStep = TRUE;
  faultContext->cr2 = virtualAddress;

  /* single step mode */

  StackPointer[0x03] |= 1 << 8;

  /* traced, mark valid */

  ia32MarkPageValid(virtualAddress);

  /* handled with error code */

  status = 0;

  /* decode insn */

  insn = &faultContext->event.insn;

  insnSize = disasmDecodeMovInsn(&faultContext->disasmContext, StackPointer[0x01], SavedRegisters, insn);
  if (!insnSize)
    {
      DIOT_DEBUG_ERROR("disasmDecodeMovInsn(@0x%08x)\n", (ULONG_PTR)StackPointer[0x01]);
      goto onError;
    }

  if (insn->insnType == DIOT_INSN_TYPE_MEM_TO_MEM)
    if (insn->srcAddress != virtualAddress)
      {
	/* source address is not the memory mapped
	   one. it is safe to deref, wont have any
	   side effect on the device state.
	*/

	insn->operandValue = readValueAt(insn->srcAddress, insn->operandSize / 8);
      }

  faultContext->event.mapping.physicalAddress.QuadPart = mappingPhysicalAddress.QuadPart;
  faultContext->event.mapping.virtualAddress = mappingVirtualAddress;
  faultContext->event.mapping.numberOfBytes = mappingNumberOfBytes;

 onError:

  HookUnrefInterruptGate(KiTrap0eHookContext);

  return status;
}



/* hooks
 */

static void removeHooks(VOID)
{
  if (READ_PORT_UCHAR_HookContext != NULL)
    {
      HookUnsetFunction(READ_PORT_UCHAR_HookContext);
      READ_PORT_UCHAR_HookContext = NULL;
    }

  if (READ_PORT_USHORT_HookContext != NULL)
    {
      HookUnsetFunction(READ_PORT_USHORT_HookContext);
      READ_PORT_USHORT_HookContext = NULL;
    }

  if (READ_PORT_ULONG_HookContext != NULL)
    {
      HookUnsetFunction(READ_PORT_ULONG_HookContext);
      READ_PORT_ULONG_HookContext = NULL;
    }

  if (READ_PORT_BUFFER_UCHAR_HookContext != NULL)
    {
      HookUnsetFunction(READ_PORT_BUFFER_UCHAR_HookContext);
      READ_PORT_BUFFER_UCHAR_HookContext = NULL;
    }

  if (READ_PORT_BUFFER_USHORT_HookContext != NULL)
    {
      HookUnsetFunction(READ_PORT_BUFFER_USHORT_HookContext);
      READ_PORT_BUFFER_USHORT_HookContext = NULL;
    }

  if (READ_PORT_BUFFER_ULONG_HookContext != NULL)
    {
      HookUnsetFunction(READ_PORT_BUFFER_ULONG_HookContext);
      READ_PORT_BUFFER_ULONG_HookContext = NULL;
    }

  if (WRITE_PORT_UCHAR_HookContext != NULL)
    {
      HookUnsetFunction(WRITE_PORT_UCHAR_HookContext);
      WRITE_PORT_UCHAR_HookContext = NULL;
    }

  if (WRITE_PORT_USHORT_HookContext != NULL)
    {
      HookUnsetFunction(WRITE_PORT_USHORT_HookContext);
      WRITE_PORT_USHORT_HookContext = NULL;
    }

  if (WRITE_PORT_ULONG_HookContext != NULL)
    {
      HookUnsetFunction(WRITE_PORT_ULONG_HookContext);
      WRITE_PORT_ULONG_HookContext = NULL;
    }

  if (WRITE_PORT_BUFFER_UCHAR_HookContext != NULL)
    {
      HookUnsetFunction(WRITE_PORT_BUFFER_UCHAR_HookContext);
      WRITE_PORT_BUFFER_UCHAR_HookContext = NULL;
    }

  if (WRITE_PORT_BUFFER_USHORT_HookContext != NULL)
    {
      HookUnsetFunction(WRITE_PORT_BUFFER_USHORT_HookContext);
      WRITE_PORT_BUFFER_USHORT_HookContext = NULL;
    }

  if (WRITE_PORT_BUFFER_ULONG_HookContext != NULL)
    {
      HookUnsetFunction(WRITE_PORT_BUFFER_ULONG_HookContext);
      WRITE_PORT_BUFFER_ULONG_HookContext = NULL;
    }

  if (MmMapIoSpaceHookContext != NULL)
    {
      HookUnsetFunction(MmMapIoSpaceHookContext);
      MmMapIoSpaceHookContext = NULL;
    }

  if (MmUnmapIoSpaceHookContext != NULL)
    {
      HookUnsetFunction(MmUnmapIoSpaceHookContext);
      MmUnmapIoSpaceHookContext = NULL;
    }

  if (KiTrap0eHookContext != NULL)
    {
      HookUnsetInterruptGate(KiTrap0eHookContext);
      KiTrap0eHookContext = NULL;
    }

  if (KiTrap01HookContext != NULL)
    {
      HookUnsetInterruptGate(KiTrap01HookContext);
      KiTrap01HookContext = NULL;
    }
}


static NTSTATUS installMmHooks(void)
{
  /* install MmXxx routines related hooks
   */

  MmMapIoSpaceHookContext = HookSetFunction((ULONG_PTR)MmMapIoSpace, (ULONG_PTR)_MmMapIoSpace, 0x10);
  if (MmMapIoSpaceHookContext == NULL)
    goto onError;

  MmUnmapIoSpaceHookContext = HookSetFunction((ULONG_PTR)MmUnmapIoSpace, (ULONG_PTR)_MmUnmapIoSpace, 0x08);
  if (MmUnmapIoSpaceHookContext == NULL)
    goto onError;

  return STATUS_SUCCESS;
  
 onError:

  if (MmMapIoSpaceHookContext != NULL)
    {
      HookUnsetFunction(MmMapIoSpaceHookContext);
      MmMapIoSpaceHookContext = NULL;
    }

  if (MmUnmapIoSpaceHookContext != NULL)
    {
      HookUnsetFunction(MmUnmapIoSpaceHookContext);
      MmUnmapIoSpaceHookContext = NULL;
    }

  return STATUS_UNSUCCESSFUL;
}


static BOOLEAN areIntHooksInstalled(void)
{
  /* todo: find a way to prevent race
   */

  return KiTrap0eHookContext == NULL ? FALSE : TRUE;
}


static NTSTATUS installIntHooks(void)
{
  /* install interrupt gate related hooks
   */

  initializeFaultContexts();

  KiTrap01HookContext = HookSetInterruptGate(ia32GetIdtHandler(0x01), (ULONG_PTR)_KiTrap01);
  if (KiTrap01HookContext == NULL)
    goto onError;

  KiTrap0eHookContext = HookSetInterruptGate(ia32GetIdtHandler(0x0e), (ULONG_PTR)_KiTrap0e);
  if (KiTrap0eHookContext == NULL)
    goto onError;

  return STATUS_SUCCESS;

 onError:

  if (KiTrap01HookContext != NULL)
    {
      HookUnsetInterruptGate(KiTrap01HookContext);
      KiTrap01HookContext = NULL;
    }

  if (KiTrap0eHookContext != NULL)
    {
      HookUnsetInterruptGate(KiTrap0eHookContext);
      KiTrap0eHookContext = NULL;
    }

  return STATUS_UNSUCCESSFUL;
}



/* main
 */

static void cleanup(void)
{
  removeHooks();
  eventQueueCleanup();
  comCleanup();
  ioportCleanup();
  mmioCleanup();
}


static VOID DriverUnload(PDRIVER_OBJECT Driver)
{
  cleanup();
}


NTSTATUS DriverEntry(PDRIVER_OBJECT Driver, PUNICODE_STRING RegistryPath)
{
  NTSTATUS status = STATUS_UNSUCCESSFUL;

  RtlZeroMemory(Driver->MajorFunction, sizeof(Driver->MajorFunction));

  Driver->DriverUnload = NULL;

  if (isSafeBoot() == TRUE)
    return STATUS_UNSUCCESSFUL;

  status = eventQueueInitialize();
  if (status != STATUS_SUCCESS)
    goto onError;

  status = comInitialize(Driver);
  if (status != STATUS_SUCCESS)
    goto onError;

  status = ioportInitialize();
  if (status != STATUS_SUCCESS)
    goto onError;

  status = mmioInitialize();
  if (status != STATUS_SUCCESS)
    goto onError;

  status = installPortHooks();
  if (status != STATUS_SUCCESS)
    goto onError;

  status = installMmHooks();
  if (status != STATUS_SUCCESS)
    goto onError;

  Driver->DriverUnload = DriverUnload;

  return STATUS_SUCCESS;

 onError:

  cleanup();

  return status;
}
