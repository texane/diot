/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 22:59:03 2009 texane
** Last update Wed Apr 15 22:59:05 2009 texane
*/



#ifndef DIOT_TYPES_H_INCLUDED
# define DIOT_TYPES_H_INCLUDED



#ifdef DIOT_BUILD_SYS
# include <wdm.h>
#else
# include <windows.h>
#endif



/* error
 */

enum diotError
  {
    DIOT_ERROR_SUCCESS = 0,
    DIOT_ERROR_ALREADY_INITIALIZED,
    DIOT_ERROR_NOT_INITIALIZED,
    DIOT_ERROR_NOT_FOUND,
    DIOT_ERROR_FAILURE
  };

#define DIOT_ERROR_IS_SUCCESS(E) ((E) == DIOT_ERROR_SUCCESS)



/* abstract insn
 */

struct diotInsn
{
  ULONG_PTR insnAddress;

  /* insnSize in bits */

  UCHAR insnSize;

#define DIOT_INSN_TYPE_MEM_TO_REG 0
#define DIOT_INSN_TYPE_IMM_TO_MEM 1
#define DIOT_INSN_TYPE_REG_TO_MEM 2
#define DIOT_INSN_TYPE_MEM_TO_MEM 3
#define DIOT_INSN_TYPE_INVALID 4

  UCHAR insnType;

  UCHAR operandSize;

#define DIOT_REG_EDI 0
#define DIOT_REG_ESI 1
#define DIOT_REG_EBP 2
#define DIOT_REG_ESP 3
#define DIOT_REG_EBX 4
#define DIOT_REG_EDX 5
#define DIOT_REG_ECX 6
#define DIOT_REG_EAX 7
#define DIOT_REG_BAD 8

  ULONG_PTR srcAddress;
  ULONG_PTR dstAddress;

  ULONG_PTR operandValue;

  UCHAR insnBuffer[32];
};



/* memory mapping
 */

struct diotMapping
{
  ULARGE_INTEGER physicalAddress;
  ULONG_PTR virtualAddress;
  SIZE_T numberOfBytes;
};



/* events
 */

enum diotEventType
  {
    DIOT_EVENT_ERROR = 0,
    DIOT_EVENT_TRACING_STATUS,
    DIOT_EVENT_MMIO_INSN,
    DIOT_EVENT_MMIO_MAPPING,
    DIOT_EVENT_IOPORT,
    DIOT_EVENT_INVALID
  };


enum diotTracingStatus
  {
    DIOT_TRACING_STARTED = 0,
    DIOT_TRACING_STOPPED
  };


struct diotMmioInsnEvent
{
  struct diotMapping mapping;
  struct diotInsn insn;
};


struct diotMmioMappingEvent
{
  BOOLEAN isCreated;
  struct diotMapping mapping;
};


struct diotIoportEvent
{
  BOOLEAN isInput;
  SIZE_T count;
  UCHAR size;
  ULONG_PTR port;
  ULONG_PTR value;
};


struct diotEvent
{
  /* event descriptor */

  enum diotEventType type;

  SIZE_T size;

  ULARGE_INTEGER timestamp;

  union
  {
    enum diotError error;
    struct diotMmioInsnEvent insnEvent;
    struct diotIoportEvent ioportEvent;
    struct diotMmioMappingEvent mappingEvent;
    enum diotTracingStatus tracingStatus;
  } data;

};



/* byte range
 */

struct diotRange
{
  /* lastByte is inclusive */

  ULARGE_INTEGER firstByte;
  ULARGE_INTEGER lastByte;
};



/* global conf
 */

struct diotConf
{
  BOOLEAN isEnabled;
};



#endif /* ! DIOT_TYPES_H_INCLUDED */
