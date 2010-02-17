/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 23:00:03 2009 texane
** Last update Mon Apr 20 08:13:18 2009 texane
*/



#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "../../common/diotApi.h"
#include "../../common/diotTypes.h"



/* event handling
 */

/* static const char* getDiotRegName(unsigned int r) */
/* { */
/*   const char* s; */

/*   switch (r) */
/*     { */
/*     case DIOT_REG_EDI: */
/*       s = "edi"; */
/*       break; */

/*     case DIOT_REG_ESI: */
/*       s = "esi"; */
/*       break; */

/*     case DIOT_REG_EBP: */
/*       s = "ebp"; */
/*       break; */

/*     case DIOT_REG_ESP: */
/*       s = "esp"; */
/*       break; */

/*     case DIOT_REG_EBX: */
/*       s = "ebx"; */
/*       break; */

/*     case DIOT_REG_EDX: */
/*       s = "edx"; */
/*       break; */

/*     case DIOT_REG_ECX: */
/*       s = "ecx"; */
/*       break; */

/*     case DIOT_REG_EAX: */
/*       s = "eax"; */
/*       break; */

/*     default: */
/*       s = "r" */
/*       break; */
/*     } */

/*   return s; */
/* } */


static void printInvalidInsn(const struct diotInsn* Insn)
{
  const UCHAR* buffer = Insn->insnBuffer;
  SIZE_T i;

  for (i = 0; i < Insn->insnSize; ++i)
    {
      printf(" %02x", *buffer);
      ++buffer;
    }
}


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
      printInvalidInsn(Insn);
      return ;
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

  printf("@0x%08x (%s) %s[0x%08x] <- %s[0x%08x] == 0x%08x",
	 Insn->insnAddress,
	 sizeString,
	 dstType,
	 Insn->dstAddress,
	 srcType,
	 Insn->srcAddress,
	 Insn->operandValue);
}


static void printDiotMapping(const struct diotMapping* Mapping)
{
  printf("paddr=%I64x vaddr=0x%08x size=0x%08x",
	 Mapping->physicalAddress.QuadPart,
	 Mapping->virtualAddress,
	 Mapping->numberOfBytes);
}


static const char* tracingStatusToString(enum diotTracingStatus Status)
{
  const char* statusString;

  switch (Status)
    {
    case DIOT_TRACING_STARTED:
      statusString = "started";
      break;

    case DIOT_TRACING_STOPPED:
      statusString = "stopped";
      break;

    default:
      statusString = "unknown";
      break;
    }

  return statusString;
}


static void onDiotEvent(const struct diotEvent* Event, void* Context)
{
  UNREFERENCED_PARAMETER(Context);

  printf("[%I64x]: ", Event->timestamp.QuadPart);

  switch (Event->type)
    {
    case DIOT_EVENT_ERROR:
      {
	printf("error\n");
	printf(" [+] value: 0x%08x\n", Event->data.error);
	break;
      }

    case DIOT_EVENT_TRACING_STATUS:
      {
	printf("tracingStatus\n");
	printf(" [+] value: 0x%08x\n", tracingStatusToString(Event->data.tracingStatus));
	break;
      }

    case DIOT_EVENT_MMIO_INSN:
      {
	ULONG_PTR mappingOffset;

	printf("mmioInsn\n");

	printf(" [+] mapping: ");
	printDiotMapping(&Event->data.insnEvent.mapping);
	printf("\n");

	/* bug: MEM_TO_MEM case not well handled since
	   operand hitting the mapping is not known.
	 */

#define IS_STORE_INSN(I) ((I)->insnType != DIOT_INSN_TYPE_MEM_TO_REG)

	if (IS_STORE_INSN(&Event->data.insnEvent.insn))
	  {
	    mappingOffset =
	      Event->data.insnEvent.insn.dstAddress -
	      Event->data.insnEvent.mapping.virtualAddress;
	  }
	else
	  {
	    mappingOffset =
	      Event->data.insnEvent.insn.srcAddress -
	      Event->data.insnEvent.mapping.virtualAddress;
	  }

	printf(" [+] offset: 0x%08x", mappingOffset);
	printf("\n");

	printf(" [+] insn: ");
	printDiotInsn(&Event->data.insnEvent.insn);
	printf("\n");

	break;
      }

    case DIOT_EVENT_MMIO_MAPPING:
      {
	printf("mmioMapping\n");

	printf(" [+] isCreated: %u\n", Event->data.mappingEvent.isCreated);

	printf(" [+] mapping: ");
	printDiotMapping(&Event->data.mappingEvent.mapping);
	printf("\n");

	break;
      }

    case DIOT_EVENT_IOPORT:
      {
	printf("ioport\n");

	printf(" [+] dir: %s\n", Event->data.ioportEvent.isInput == TRUE ? "in" : "out");
	printf(" [+] count: 0x%x\n", Event->data.ioportEvent.count);
	printf(" [+] size: 0x%x\n", Event->data.ioportEvent.size);
	printf(" [+] port: 0x%04x\n", Event->data.ioportEvent.port);

	printf(" [+] value: %c0x%08x\n",
	       Event->data.ioportEvent.count == 1 ? '$' : '@',
	       Event->data.ioportEvent.value);

	break;
      }
      
    case DIOT_EVENT_INVALID:
    default:
      {
	printf("invalid\n");
	break;
      }
    }

  printf("\n");
}



/* range parsing
 */

static int strToUlong(const char* s, PULONG n)
{
  char* p;

  errno = 0;

  *n = strtoul(s, &p, 16);

  if (errno == ERANGE)
    return -1;

  if (p != (s + strlen(s)))
    return -1;

  return 0;
}


static int getRange(const char* s, struct diotRange* r)
{
  char buffer[32];
  unsigned int i;

  for (i = 0; i < 10; ++i)
    if (!s[i] || (s[i] == '-'))
      break;

  if (i == 10 || s[i] != '-')
    goto onError;

  memcpy(buffer, s, i);
  buffer[i] = 0;

  r->firstByte.HighPart = 0;

  if (strToUlong(buffer, &r->firstByte.LowPart) == -1)
    goto onError;

  r->lastByte.HighPart = 0;

  if (strToUlong(s + i + 1, &r->lastByte.LowPart) == -1)
    goto onError;

  return 0;

 onError:

  return -1;
}



/* main
 */

int main(int ac, char** av)
{
  BOOL isSuccess = FALSE;
  HANDLE waitEvent = NULL;
  struct diotRange* currentRange;
  struct diotRange* mmioRanges = NULL;
  struct diotRange* ioportRanges = NULL;
  ULONG ioportRangeCount = 0;
  ULONG mmioRangeCount = 0;
  ULONG rangeCount;
  enum diotError diotError;
  ULONG rangeIndex;
  struct diotConf conf;

  if (ac == 1)
    {
      printf("usage: %s (-{p|m}:<first-last>)+\n", *av);
      goto onError;
    }

  /* at most ac - 1 ranges */

  rangeCount = ac - 1;

  ioportRanges = malloc(rangeCount * sizeof(struct diotRange));
  if (ioportRanges == NULL)
    goto onError;

  mmioRanges = malloc(rangeCount * sizeof(struct diotRange));
  if (mmioRanges == NULL)
    goto onError;

  for (rangeIndex = 0; rangeIndex < rangeCount; ++rangeIndex)
    {
      if (strlen(av[rangeIndex + 1]) <= 3)
	goto onError;

      if (av[rangeIndex + 1][0] != '-')
	goto onError;

      if (av[rangeIndex + 1][2] != ':')
	goto onError;

      if (av[rangeIndex + 1][1] == 'p')
	currentRange = &ioportRanges[ioportRangeCount++];
      else if (av[rangeIndex + 1][1] == 'm')
	currentRange = &mmioRanges[mmioRangeCount++];
      else
	goto onError;

      if (getRange(&av[rangeIndex + 1][3], currentRange) == -1)
	{
	  printf("invalidRange(%s) == -1\n", av[rangeIndex + 1]);
	  goto onError;
	}
    }

  waitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (waitEvent == NULL)
    {
      printf("CreateEvent() == 0x%x\n", GetLastError());
      goto onError;
    }

  diotError = diotInitialize(onDiotEvent, &waitEvent);
  if (!DIOT_ERROR_IS_SUCCESS(diotError))
    {
      printf("diotInitialize() == 0x%x\n", diotError);
      goto onError;
    }

  ZeroMemory(&conf, sizeof(struct diotConf));
  conf.isEnabled = TRUE;

  diotError = diotSetConf(&conf);
  if (!DIOT_ERROR_IS_SUCCESS(diotError))
    {
      printf("diotSetConf() == 0x%x\n", diotError);
      goto onError;
    }

  if (mmioRangeCount)
    {
      diotError = diotSetMmioRanges(mmioRanges, mmioRangeCount);
      if (!DIOT_ERROR_IS_SUCCESS(diotError))
	{
	  printf("diotSetMmioRanges() == 0x%x\n", diotError);
	  goto onError;
	}
    }

  if (ioportRangeCount)
    {
      diotError = diotSetIoportRanges(ioportRanges, ioportRangeCount);
      if (!DIOT_ERROR_IS_SUCCESS(diotError))
	{
	  printf("diotSetIoportRanges() == 0x%x\n", diotError);
	  goto onError;
	}
    }

  diotError = diotStartTracing();
  if (!DIOT_ERROR_IS_SUCCESS(diotError))
    {
      printf("diotStartTracing() == 0x%x\n", diotError);
      goto onError;
    }

  WaitForSingleObject(waitEvent, INFINITE);

  diotStopTracing();

  ZeroMemory(&conf, sizeof(struct diotConf));
  conf.isEnabled = FALSE;

  diotSetConf(&conf);

  /* success */

  isSuccess = TRUE;

 onError:

  diotCleanup();

  if (waitEvent != NULL)
    CloseHandle(waitEvent);

  if (ioportRanges != NULL)
    free(ioportRanges);

  if (mmioRanges != NULL)
    free(mmioRanges);

  return isSuccess == TRUE ? 0 : -1;
}
