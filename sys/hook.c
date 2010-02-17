/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 22:57:06 2009 texane
** Last update Wed Apr 15 22:57:09 2009 texane
*/



#include <wdm.h>
#include "Zdisasm.h"
#include "hook.h"
#include "../common/diotDebug.h"



__declspec(naked) static void intGateHookPattern(void)
{
  __asm
    {
      ; save context ;
      pushfd ;

      pushad ;
      push ds ;
      push es ;
      push fs ;

      mov ebx, 0x23 ;
      mov ds, bx ;
      mov es, bx ;

      mov ebx, 0x30 ;
      mov fs, bx ;

      ; stack pointer ;
      mov eax, esp ;
      mov ebx, eax ;
      add eax, 0x30 ;
      push eax ;

      ; saved registers ;
      add ebx, 0x0c ;
      push ebx ;

      ; pattern size must be REL_CALL_INSN_SIZE ;
      ; patched by rel call to payloadRoutine ;

      __emit 0x2a ;
      __emit 0x2a ;
      __emit 0x2a ;
      __emit 0x2a ;
      __emit 0x2a ;

      ; payload cleans the stack (__stdcall) ;

      ; page fault handled ;
      and eax, eax ;

      ; restore context ;
      pop fs ;
      pop es ;
      pop ds ;
      popad ;

      js notHandled ;
      jz handledWithErrorCode ;

      ; handledNoErrorCode:
      ; restore context (eflags) ;
      popfd ;

      iretd ;

    handledWithErrorCode:

      ; restore context (eflags) ;
      popfd ;

      ; remove error code (lea does not mod eflags) ;
      lea esp, dword ptr [ esp + 4 ] ;

      iretd ;

    notHandled:

      ; restore context (eflags) ;
      popfd ;

      ; this pattern gets replaced with original bytes ;
      ; plus a far jmp to the orignal routine instruction ;
      ; pattern size does not matter ;

      __emit 0x2b ;
      __emit 0x2b ;
      __emit 0x2b ;
      __emit 0x2b ;
    }
}


static void makeNearRet(PUCHAR bufAddr, SIZE_T Operand)
{
#define NEAR_RET_INSN_SIZE 3

  static const UCHAR codePattern[] = { 0xc2, 0xbb, 0xbb };

  memcpy(bufAddr, codePattern, NEAR_RET_INSN_SIZE);

  *(PUSHORT)(bufAddr + 1) = (USHORT)Operand;
}


static void makeFarJmp(PUCHAR bufAddr, ULONG_PTR jmpAddr)
{
#define FAR_JMP_INSN_SIZE 7

  static const UCHAR codePattern[] = { 0xea, 0xbb, 0xbb, 0xbb, 0xbb, 0x08, 0x00 };

  memcpy(bufAddr, codePattern, FAR_JMP_INSN_SIZE);

  *(PULONG)(bufAddr + 1) = jmpAddr;
}


static void makeFarCall(PUCHAR bufAddr, ULONG_PTR targetAddr)
{
#define FAR_CALL_INSN_SIZE 7

  static const UCHAR codePattern[] = { 0x9a, 0xbb, 0xbb, 0xbb, 0xbb, 0x08, 0x00 };

  memcpy(bufAddr, codePattern, FAR_CALL_INSN_SIZE);

  *(PULONG)(bufAddr + 1) = targetAddr;
}


static void makeIndirectJmp(PUCHAR bufAddr, ULONG_PTR jmpAddr)
{
#define INDIRECT_JMP_INSN_SIZE 6

  static const UCHAR codePattern[] = { 0xff, 0x25, 0xbb, 0xbb, 0xbb, 0xbb };

  memcpy(bufAddr, codePattern, INDIRECT_JMP_INSN_SIZE);

  *(PULONG)(bufAddr + 2) = jmpAddr;
}


static void makeRelativeCall(PUCHAR patchAddr, ULONG_PTR srcAddr, ULONG_PTR targetAddr)
{
#define REL_CALL_INSN_SIZE 5
  static const UCHAR codePattern[] = { 0xe8, 0x00, 0x00, 0x00, 0x00 };

  LONGLONG offset;

  if (!srcAddr)
    {
      /* same addresses */

      srcAddr = (ULONG_PTR)patchAddr;
    }

  memcpy(patchAddr, codePattern, REL_CALL_INSN_SIZE);

  if (targetAddr > (ULONG_PTR)patchAddr)
    offset = targetAddr - ((ULONG_PTR)patchAddr + REL_CALL_INSN_SIZE);
  else
    offset = ((LONGLONG)patchAddr + REL_CALL_INSN_SIZE - targetAddr) * -1;

  *(PULONG)(patchAddr + 1) = (ULONG)offset;
}


static void makeRelativeJmp(PUCHAR patchAddr, ULONG_PTR srcAddr, ULONG_PTR targetAddr)
{
#define REL_JMP_INSN_SIZE 5
  static const UCHAR codePattern[] = { 0xe9, 0x00, 0x00, 0x00, 0x00 };

  LONGLONG offset;

  if (!srcAddr)
    {
      /* same addresses */

      srcAddr = (ULONG_PTR)patchAddr;
    }

  memcpy(patchAddr, codePattern, REL_JMP_INSN_SIZE);

  if (targetAddr > (ULONG_PTR)patchAddr)
    offset = targetAddr - ((ULONG_PTR)srcAddr + REL_JMP_INSN_SIZE);
  else
    offset = ((LONGLONG)srcAddr + REL_JMP_INSN_SIZE - targetAddr) * -1;

  *(PULONG)(patchAddr + 1) = (ULONG)offset;
}


/* interrupt gate hooks
 */

typedef struct _INTGATE_HOOK_CONTEXT
{
  __declspec(align(4)) volatile LONG refCount;

  SIZE_T originalSize;
  ULONG_PTR originalAddress;

  SIZE_T hookSize;

#define HOOK_BUFFER_SIZE 512
  UCHAR hookBuffer[HOOK_BUFFER_SIZE];

#define ORIGINAL_BUFFER_SIZE 64
  UCHAR originalBuffer[ORIGINAL_BUFFER_SIZE];

} INTGATE_HOOK_CONTEXT, *PINTGATE_HOOK_CONTEXT;


PHOOK_CONTEXT HookSetInterruptGate(ULONG_PTR OriginalRoutine, ULONG_PTR PayloadRoutine)
{
  NTSTATUS status = STATUS_UNSUCCESSFUL;
  PMDL mdl = NULL;
  BOOLEAN isMdlMapped = FALSE;
  BOOLEAN isMdlLocked = FALSE;
  PUCHAR mappedAddress = NULL;

  PINTGATE_HOOK_CONTEXT context = NULL;

  UCHAR* ptr;
  ULONG insnLength;
  ULONG hookOffset;
  ULONG hookSize;
  ULONG origOffset;
  PUCHAR hookBuffer;

  KIRQL irql;

  /* create a writable mapping */

  mdl = IoAllocateMdl((PVOID)OriginalRoutine, ORIGINAL_BUFFER_SIZE, FALSE, FALSE, NULL);
  if (mdl == NULL)
    goto onError;

  __try
    {
      MmProbeAndLockPages(mdl, KernelMode, IoModifyAccess);
    }
  __except(EXCEPTION_EXECUTE_HANDLER)
    {
      goto onError;
    }

  isMdlLocked = TRUE;

  mappedAddress = MmMapLockedPagesSpecifyCache(mdl, KernelMode, MmNonCached, NULL, FALSE, HighPagePriority);
  if (mappedAddress == NULL)
    goto onError;

  isMdlMapped = TRUE;

  /* allocate context */

  context = ExAllocatePoolWithTag(NonPagedPool, sizeof(INTGATE_HOOK_CONTEXT), DIOT_POOL_TAG);
  if (context == NULL)
    goto onError;

  context->refCount = 1;

  hookBuffer = context->hookBuffer;

  /* originalRoutine insn count */

  ptr = mappedAddress;

  origOffset = 0;

  while (origOffset < FAR_JMP_INSN_SIZE)
    {
      GetInstLenght((PVOID)ptr, &insnLength);
      if (!insnLength)
	break;

      ptr += insnLength;

      origOffset += insnLength;
    }

  if (origOffset < FAR_JMP_INSN_SIZE)
    goto onError;

  /* make hook buffer */

  ptr = (UCHAR*)intGateHookPattern;

  hookOffset = 0;

  for (hookSize = 0; hookSize < HOOK_BUFFER_SIZE; ++hookSize)
    {
      if (!hookOffset && *(const ULONG*)ptr == 0x2a2a2a2a)
	hookOffset = hookSize;

      if (*(const ULONG*)ptr == 0x2b2b2b2b)
	break ;

      ++ptr;
    }

  if (!hookOffset || (hookSize == HOOK_BUFFER_SIZE))
    goto onError;

  memcpy(hookBuffer, (PVOID)intGateHookPattern, hookSize);
  memcpy(hookBuffer + hookSize, mappedAddress, origOffset);
  makeFarJmp(hookBuffer + hookSize + origOffset, OriginalRoutine + origOffset);
  makeRelativeCall(hookBuffer + hookOffset, (ULONG_PTR)NULL, PayloadRoutine);

  /* fill hook context */

  context->originalSize = origOffset;
  memcpy(context->originalBuffer, mappedAddress, origOffset);

  context->originalAddress = OriginalRoutine;
  context->hookSize = hookSize + origOffset + FAR_JMP_INSN_SIZE;

  /* patch originalRoutine */

  KeRaiseIrql(DISPATCH_LEVEL, &irql);

  makeFarJmp(mappedAddress, (ULONG_PTR)hookBuffer);

  KeLowerIrql(irql);

  status = STATUS_SUCCESS;

 onError:

  if (mdl != NULL)
    {
      if (isMdlMapped == TRUE)
	MmUnmapLockedPages(mappedAddress, mdl);

      if (isMdlLocked == TRUE)
	MmUnlockPages(mdl);

      IoFreeMdl(mdl);
    }

  if (status != STATUS_SUCCESS)
    {
      if (context != NULL)
	{
	  ExFreePoolWithTag(context, DIOT_POOL_TAG);
	  context = NULL;
	}
    }

  return context;
}


VOID HookUnsetInterruptGate(PHOOK_CONTEXT Context)
{
  PINTGATE_HOOK_CONTEXT const context = Context;

  PVOID mappedAddress = NULL;
  PMDL mdl = NULL;
  BOOLEAN isMdlLocked = FALSE;
  BOOLEAN isMdlMapped = FALSE;
  LARGE_INTEGER interval;
  KIRQL irql;

  mdl = IoAllocateMdl((PVOID)context->originalAddress, context->originalSize, FALSE, FALSE, NULL);
  if (mdl == NULL)
    return ;

  __try
    {
      MmProbeAndLockPages(mdl, KernelMode, IoModifyAccess);
    }
  __except(EXCEPTION_EXECUTE_HANDLER)
    {
      goto onError;
    }

  isMdlLocked = TRUE;

  mappedAddress = MmMapLockedPagesSpecifyCache(mdl, KernelMode, MmNonCached, NULL, FALSE, HighPagePriority);
  if (mappedAddress == NULL)
    goto onError;

  isMdlMapped = TRUE;

  KeRaiseIrql(DISPATCH_LEVEL, &irql);

  memcpy(mappedAddress, context->originalBuffer, context->originalSize);

  KeLowerIrql(irql);

  /* wait for the last ref and wait for 1
     second before returning to the caller
   */

  while (context->refCount > 1)
    ;

  context->refCount = 0;

  interval.QuadPart = -10000000LL;

  KeDelayExecutionThread(KernelMode, FALSE, &interval);

  ExFreePoolWithTag(context, DIOT_POOL_TAG);

 onError:

  if (mdl != NULL)
    {
      if (isMdlMapped == TRUE)
	MmUnmapLockedPages(mappedAddress, mdl);

      if (isMdlLocked == TRUE)
	MmUnlockPages(mdl);

      IoFreeMdl(mdl);
    }
}


VOID HookRefInterruptGate(PHOOK_CONTEXT Context)
{
  PINTGATE_HOOK_CONTEXT const context = Context;

  ++context->refCount;
}


VOID HookUnrefInterruptGate(PHOOK_CONTEXT Context)
{
  PINTGATE_HOOK_CONTEXT const context = Context;

  --context->refCount;
}



/* function hooks
 */

typedef struct _FUNCTION_HOOK_CONTEXT
{
  __declspec(align(4)) volatile LONG refCount;
  PVOID hookBuffer;
  PVOID callBuffer;
  PVOID origBuffer;
  SIZE_T origSize;
  ULONG_PTR origAddress;
} FUNCTION_HOOK_CONTEXT, *PFUNCTION_HOOK_CONTEXT;


__declspec(naked) static ULONG_PTR __fastcall functionCallPattern(ULONG_PTR OriginalStackPointer)
{
  __asm
    {
      pushad ;

      ; recopy the original stack (fastcall, in ecx) ;
      mov esi, ecx ;
      sub esp, 0x2b2b2b2b ;
      mov edi, esp ;
      mov ecx, 0x2b2b2b2b ;
      shr ecx, 2 ;
      rep movsd ;

      ; compute retaddr ;
      call label0 ;
    label0: ;
      pop eax ;
      sub eax, label0 ;
      add eax, label1 ;
      push eax ;

      jmp label2 ;

    label1: ;
      ; restore (preserve eax) ;
      mov dword ptr [esp + 7 * 4], eax ;

      popad ;
      ret ;

    label2: ;
      ; jmp OriginalRoutine ;
      __emit 0x2a ;
      __emit 0x2a ;
      __emit 0x2a ;
      __emit 0x2a ;
    }
}


__declspec(naked) static void functionHookPattern(void)
{
  __asm
    {
      ; save gregs ;
      pushad ;

      ; OriginalStackPointer ;
      mov eax, esp ;
      add eax, 0x24 ;
      push eax ;

      ; call payloadRoutine(__stdcall) ;
      __emit 0x2a ;
      __emit 0x2a ;
      __emit 0x2a ;
      __emit 0x2a ;
      __emit 0x2a ;

      ; restore gregs (preserve eax) ;
      mov dword ptr [esp + 7 * 4], eax ;

      popad ;

      ; near ret ParamSize ;

      __emit 0x2b ;
      __emit 0x2b ;
      __emit 0x2b ;
      __emit 0x2b ;
    }
}


PHOOK_CONTEXT HookSetFunction(ULONG_PTR OriginalRoutine, ULONG_PTR PayloadRoutine, SIZE_T ParamSize)
{
  PUCHAR callBuffer = NULL;
  PUCHAR hookBuffer = NULL;
  PUCHAR origBuffer = NULL;

  PMDL mdl = NULL;
  BOOLEAN isMdlMapped = FALSE;
  BOOLEAN isMdlLocked = FALSE;
  PUCHAR mappedAddress = NULL;

  BOOLEAN isSuccess = FALSE;

  PFUNCTION_HOOK_CONTEXT context;
  SIZE_T hookOffset;
  SIZE_T origOffset;
  SIZE_T size;
  SIZE_T index;
  PUCHAR ptr;

  KIRQL irql;

  /* create a writable mapping */

  mdl = IoAllocateMdl((PVOID)OriginalRoutine, ORIGINAL_BUFFER_SIZE, FALSE, FALSE, NULL);
  if (mdl == NULL)
    goto onError;

  __try
    {
      MmProbeAndLockPages(mdl, KernelMode, IoModifyAccess);
    }
  __except(EXCEPTION_EXECUTE_HANDLER)
    {
      goto onError;
    }

  isMdlLocked = TRUE;

  mappedAddress = MmMapLockedPagesSpecifyCache(mdl, KernelMode, MmNonCached, NULL, FALSE, HighPagePriority);
  if (mappedAddress == NULL)
    goto onError;

  isMdlMapped = TRUE;

  /* get OriginalRoutine offset */

  ptr = mappedAddress;

  origOffset = 0;

  while (origOffset < REL_JMP_INSN_SIZE)
    {
      GetInstLenght((PDWORD)ptr, (PDWORD)&size);

      if (!size)
	break;

      ptr += size;

      origOffset += size;
    }

  if (origOffset < REL_JMP_INSN_SIZE)
    goto onError;

  origBuffer = ExAllocatePoolWithTag(NonPagedPool, origOffset, DIOT_POOL_TAG);
  if (origBuffer == NULL)
    goto onError;

  memcpy(origBuffer, mappedAddress, origOffset);

  /* make functionHookPattern */

  hookOffset = 0;

  size = 0;

  ptr = (PUCHAR)functionHookPattern;

  while (*(PULONG)ptr != 0x2b2b2b2b)
    {
      if (!hookOffset && (*(PULONG)ptr == 0x2a2a2a2a))
	hookOffset = size;

      ++ptr;
      ++size;
    }

  if (!hookOffset)
    goto onError;

  hookBuffer = ExAllocatePoolWithTag(NonPagedPool, size + NEAR_RET_INSN_SIZE, DIOT_POOL_TAG);
  if (hookBuffer == NULL)
    goto onError;

  memcpy(hookBuffer, functionHookPattern, size);

  makeRelativeCall(hookBuffer + hookOffset, (ULONG_PTR)NULL, PayloadRoutine);

  makeNearRet(hookBuffer + size, ParamSize);

  /* make functionCallPattern */

  size = 0;

  ptr = (PUCHAR)functionCallPattern;

  while (*(PULONG)ptr != 0x2a2a2a2a)
    {
      ++size;
      ++ptr;
    }

  callBuffer = ExAllocatePoolWithTag(NonPagedPool, size + origOffset + REL_JMP_INSN_SIZE, DIOT_POOL_TAG);
  if (callBuffer == NULL)
    goto onError;

  memcpy(callBuffer, functionCallPattern, size);
  memcpy(callBuffer + size, mappedAddress, origOffset);
  makeRelativeJmp(callBuffer + size + origOffset, (ULONG_PTR)NULL, OriginalRoutine + origOffset);

  ptr = callBuffer;

  while (size)
    {
      if (*(PULONG)ptr == 0x2b2b2b2b)
	{
	  *(PULONG)ptr = ParamSize;
	  index = sizeof(ULONG);
	}
      else
	{
	  index = 1;
	}

      ptr += index;

      if (index > size)
	break;

      size -= index;
    }
      
  /* patch OriginalRoutine */

  KeRaiseIrql(DISPATCH_LEVEL, &irql);

  makeRelativeJmp(mappedAddress, OriginalRoutine, (ULONG_PTR)hookBuffer);

  KeLowerIrql(irql);

  /* create context */

  context = ExAllocatePoolWithTag(NonPagedPool, sizeof(FUNCTION_HOOK_CONTEXT), DIOT_POOL_TAG);
  if (context == NULL)
    goto onError;

  context->refCount = 1;

  context->hookBuffer = hookBuffer;
  context->callBuffer = callBuffer;

  context->origSize = origOffset;
  context->origBuffer = origBuffer;
  context->origAddress = OriginalRoutine;

  isSuccess = TRUE;

 onError:

  if (mdl != NULL)
    {
      if (isMdlMapped == TRUE)
	MmUnmapLockedPages(mappedAddress, mdl);

      if (isMdlLocked == TRUE)
	MmUnlockPages(mdl);

      IoFreeMdl(mdl);
    }

  if (isSuccess == TRUE)
    return context;

  if (hookBuffer != NULL)
    ExFreePoolWithTag(hookBuffer, DIOT_POOL_TAG);

  if (callBuffer != NULL)
    ExFreePoolWithTag(callBuffer, DIOT_POOL_TAG);

  if (origBuffer != NULL)
    ExFreePoolWithTag(origBuffer, DIOT_POOL_TAG);

  return NULL;
}


void HookUnsetFunction(PHOOK_CONTEXT HookContext)
{
  PFUNCTION_HOOK_CONTEXT const context = HookContext;
  PMDL mdl = NULL;
  BOOLEAN isMdlMapped = FALSE;
  BOOLEAN isMdlLocked = FALSE;
  PUCHAR mappedAddress = NULL;
  LARGE_INTEGER interval;
  KIRQL irql;

  /* create a writable mapping */

  mdl = IoAllocateMdl((PVOID)context->origAddress, context->origSize, FALSE, FALSE, NULL);
  if (mdl == NULL)
    goto onError;

  __try
    {
      MmProbeAndLockPages(mdl, KernelMode, IoModifyAccess);
    }
  __except(EXCEPTION_EXECUTE_HANDLER)
    {
      goto onError;
    }

  isMdlLocked = TRUE;

  mappedAddress = MmMapLockedPagesSpecifyCache(mdl, KernelMode, MmNonCached, NULL, FALSE, HighPagePriority);
  if (mappedAddress == NULL)
    goto onError;

  isMdlMapped = TRUE;

  /* restore bytes */

  KeRaiseIrql(DISPATCH_LEVEL, &irql);

  memcpy(mappedAddress, context->origBuffer, context->origSize);

  KeLowerIrql(irql);

  /* wait for the last ref and wait for 1
     second before returning to the caller
   */

  while (context->refCount > 1)
    ;

  context->refCount = 0;

  interval.QuadPart = -10000000LL;

  KeDelayExecutionThread(KernelMode, FALSE, &interval);

 onError:

  if (mdl != NULL)
    {
      if (isMdlMapped == TRUE)
	MmUnmapLockedPages(mappedAddress, mdl);

      if (isMdlLocked == TRUE)
	MmUnlockPages(mdl);

      IoFreeMdl(mdl);
    }

  ExFreePoolWithTag(context->hookBuffer, DIOT_POOL_TAG);
  ExFreePoolWithTag(context->callBuffer, DIOT_POOL_TAG);
  ExFreePoolWithTag(context->origBuffer, DIOT_POOL_TAG);

  ExFreePoolWithTag(context, DIOT_POOL_TAG);  
}


VOID HookRefFunction(PHOOK_CONTEXT Context)
{
  /* note: should not unref if ref failed
     todo: use interlocked to prevent races
   */

  PFUNCTION_HOOK_CONTEXT const context = Context;

  ++context->refCount;
}


VOID HookUnrefFunction(PHOOK_CONTEXT Context)
{
  PFUNCTION_HOOK_CONTEXT const context = Context;

  --context->refCount;
}


ULONG_PTR __fastcall HookCallFunction(ULONG_PTR OriginalStackPointer, PHOOK_CONTEXT HookContext)
{
  const ULONG_PTR callBuffer = (ULONG_PTR)((PFUNCTION_HOOK_CONTEXT)HookContext)->callBuffer;

  return ((ULONG_PTR (__fastcall *)(ULONG_PTR))callBuffer)(OriginalStackPointer);
}
