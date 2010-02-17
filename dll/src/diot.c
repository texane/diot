/*
** Made by texane <texane@gmail.com>
** 
** Started on  Sat Apr 18 08:16:27 2009 texane
** Last update Mon Apr 20 09:38:53 2009 texane
*/



#include <windows.h>
#include <stddef.h>
#include <stdlib.h>
#include "../../common/diotApi.h"
#include "../../common/diotCom.h"
#include "../../common/diotDebug.h"



/* thread related types
 */

struct eventItem
{
  struct eventItem* next;
  ULONG count;
  struct diotEvent events[1];
};


struct threadProcessorData
{
  HANDLE eventListMutex;
  struct eventItem* eventListTail;
  struct eventItem* eventListHead;
  void (*eventHandler)(const struct diotEvent*, void*);
  void* handlerContext;
};


struct threadControlBlock
{
  HANDLE threadHandle;
  HANDLE controlHandle;

#define CONTROL_MESSAGE_NONE 0
#define CONTROL_MESSAGE_SPECIFIC 1
#define CONTROL_MESSAGE_DONE 2

#define ATOMIC_VOLATILE_LONG __declspec(align(4)) volatile LONG
  ATOMIC_VOLATILE_LONG controlMessage;

  union
  {
    struct threadProcessorData processorData;
  } data;
};

#define STATIC_THREAD_CB_INITIALIZER { NULL, NULL, CONTROL_MESSAGE_DONE }



/* globals
 */

static BOOLEAN gIsInitialized = FALSE;
static HANDLE gDeviceHandle = INVALID_HANDLE_VALUE;
static HANDLE gComEvent = NULL;
static struct threadControlBlock gNotifierThread = STATIC_THREAD_CB_INITIALIZER;
static struct threadControlBlock gProcessorThread = STATIC_THREAD_CB_INITIALIZER;



/* thread routines
 */

static BOOLEAN createThread(struct threadControlBlock* ControlBlock, DWORD (WINAPI * ThreadEntry)(LPVOID))
{
  /* assume ControlBlock initialized */

  HANDLE event = NULL;
  HANDLE thread = NULL;

  event = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (event == NULL)
    goto onError;

  thread = CreateThread(NULL, 0, ThreadEntry, ControlBlock, CREATE_SUSPENDED, NULL);
  if (thread == NULL)
    goto onError;

  ControlBlock->threadHandle = thread;
  ControlBlock->controlHandle = event;
  ControlBlock->controlMessage = CONTROL_MESSAGE_NONE;

  if (ResumeThread(thread) == (DWORD)-1)
    {
      TerminateThread(thread, 0);
      goto onError;
    }

  return TRUE;

 onError:

  if (event != NULL)
    CloseHandle(event);
  
  if (thread != NULL)
    CloseHandle(thread);

  ControlBlock->threadHandle = NULL;
  ControlBlock->controlHandle = NULL;

  return FALSE;
}


static void destroyThread(struct threadControlBlock* ControlBlock)
{
  if (ControlBlock->threadHandle == NULL)
    return ;

  ControlBlock->controlMessage = CONTROL_MESSAGE_DONE;
  SetEvent(ControlBlock->controlHandle);
  WaitForSingleObject(ControlBlock->controlHandle, 10000);

  CloseHandle(ControlBlock->threadHandle);
  CloseHandle(ControlBlock->controlHandle);

  ControlBlock->threadHandle = NULL;
  ControlBlock->controlHandle = NULL;
}



/* iocontrol helper
 */

static BOOL doDiotDeviceControl(DWORD IoctlCode,
				const void* InputBuffer,
				DWORD InputLength,
				void* OutputBuffer,
				DWORD OutputLength)
{
  BOOL isSuccess;
  DWORD size;

  isSuccess =
    DeviceIoControl(gDeviceHandle,
		    IoctlCode,
		    (LPVOID)InputBuffer,
		    InputLength,
		    OutputBuffer,
		    OutputLength,
		    &size,
		    NULL);

  if (isSuccess == FALSE)
    {
      DIOT_DEBUG_ERROR("DeviceIoControl(0x%x) == 0x%x\n", IoctlCode, GetLastError());
      return FALSE;
    }

  return TRUE;
}



/* event notifying thread
 */

static struct eventItem* allocEventItem(ULONG Count)
{
  const SIZE_T size = offsetof(struct eventItem, events) + Count * sizeof(struct diotEvent);
  struct eventItem* item;

  item = malloc(size);
  if (item == NULL)
    return NULL;

  item->next = NULL;
  item->count = Count;

  return item;
}


static void freeEventItem(struct eventItem* item)
{
  free(item);
}


static void pushEventItem(struct eventItem* Item)
{
  /* push an item to the processor thread event list.
     if the item cannot be pushed, it is freed.
     if the item is pushed, processor thread is signaled.
     once an item is pushed, its memory is owned by the processor thread.
   */

  struct threadProcessorData* const processorData = &gProcessorThread.data.processorData;

  if (WaitForSingleObject(processorData->eventListMutex, INFINITE) != WAIT_OBJECT_0)
    {
      freeEventItem(Item);
      return ;
    }

  if (processorData->eventListHead == NULL)
    processorData->eventListHead = Item;
  else
    processorData->eventListTail->next = Item;

  processorData->eventListTail = Item;

  ReleaseMutex(processorData->eventListMutex);

  gProcessorThread.controlMessage = CONTROL_MESSAGE_SPECIFIC;
  SetEvent(gProcessorThread.controlHandle);
}


static void fetchDiotEvents(void)
{
  struct eventItem* item;
  DWORD size;
  BOOL isSuccess;

  while (1)
    {
#define DIOT_EVENT_COUNT 64
      item = allocEventItem(DIOT_EVENT_COUNT);
      if (item == NULL)
	{
	  /* todo: flush kernel events */
	  return ;
	}

      isSuccess =
	DeviceIoControl(gDeviceHandle,
			DIOT_IOCTL_GET_EVENTS,
			NULL,
			0,
			item->events,
			DIOT_EVENT_COUNT * sizeof(struct diotEvent),
			&size,
			NULL);

      if (isSuccess == FALSE || !size)
	{
	  freeEventItem(item);
	  return ;
	}

      /* update the event count */

      item->count = size / sizeof(struct diotEvent);

      pushEventItem(item);
    }
}


static DWORD WINAPI notifierEntry(LPVOID Context)
{
  struct threadControlBlock* const controlBlock = Context;
  BOOL isDone = FALSE;
  DWORD status;
  LONG controlMessage;
  HANDLE events[2];

  events[0] = gComEvent;
  events[1] = controlBlock->controlHandle;

  while (isDone == FALSE)
    {
      status = WaitForMultipleObjects(2, events, FALSE, INFINITE);
      if (status == WAIT_OBJECT_0 + 0)
	{
	  /* gComEvent */

	  fetchDiotEvents();
	}
      else if (status == WAIT_OBJECT_0 + 1)
	{
	  /* controlHandle */

	  controlMessage = InterlockedExchange(&controlBlock->controlMessage, CONTROL_MESSAGE_NONE);

	  switch (controlMessage)
	    {
	    case CONTROL_MESSAGE_DONE:
	      isDone = TRUE;
	      break;

	    default:
	      break;
	    }
	}
    }

  return 0;
}



/* event processing thread
 */

static void freeEventList(struct eventItem* head)
{
  struct eventItem* prev;

  while (head != NULL)
    {
      prev = head;
      head = head->next;

      freeEventItem(prev);
    }
}


static void processEventList(struct eventItem* head,
			     void (*EventHandler)(const struct diotEvent*, void*),
			     void* HandlerContext)
{
  const struct diotEvent* event;
  struct eventItem* prev;
  ULONG eventIndex;

  while (head != NULL)
    {
      prev = head;
      head = head->next;

      event = prev->events;

      for (eventIndex = 0; eventIndex < prev->count; ++eventIndex, ++event)
	EventHandler(event, HandlerContext);

      freeEventItem(prev);
    }
}


static DWORD WINAPI processorEntry(LPVOID Context)
{
  struct threadControlBlock* const controlBlock = Context;
  BOOL isDone = FALSE;
  DWORD status;
  struct threadProcessorData* processorData;
  struct eventItem* head;
  LONG controlMessage;

  processorData = &controlBlock->data.processorData;

  while (isDone == FALSE)
    {
      status = WaitForSingleObject(controlBlock->controlHandle, INFINITE);

      if (status == WAIT_OBJECT_0)
	{
	  controlMessage = InterlockedExchange(&controlBlock->controlMessage, CONTROL_MESSAGE_NONE);

	  switch (controlMessage)
	    {
	    case CONTROL_MESSAGE_DONE:
	      isDone = TRUE;
	      break;

	    case CONTROL_MESSAGE_SPECIFIC:
	      if (WaitForSingleObject(processorData->eventListMutex, INFINITE) == WAIT_OBJECT_0)
		{
		  head = processorData->eventListHead;

		  processorData->eventListHead = NULL;
		  processorData->eventListTail = NULL;

		  ReleaseMutex(processorData->eventListMutex);

		  if (head != NULL)
		    processEventList(head, processorData->eventHandler, processorData->handlerContext);
		}
	      break;

	    case CONTROL_MESSAGE_NONE:
	    default:
	      break;
	    }
	}
    }

  return 0;
}


static void releaseProcessorData(struct threadProcessorData* Data)
{
  struct eventItem* head;

  if (Data->eventListMutex == NULL)
    return ;

  if (WaitForSingleObject(Data->eventListMutex, INFINITE) == WAIT_OBJECT_0)
    {
      head = Data->eventListHead;
      Data->eventListHead = NULL;
      Data->eventListTail = NULL;
      ReleaseMutex(Data->eventListMutex);

      if (head != NULL)
	freeEventList(head);
    }

  CloseHandle(Data->eventListMutex);
  Data->eventListMutex = NULL;
}


static BOOLEAN initProcessorData(struct threadProcessorData* Data,
				 void (*EventHandler)(const struct diotEvent*, void*),
				 void* HandlerContext)
{
  Data->eventListMutex = CreateMutex(NULL, FALSE, NULL);
  if (Data->eventListMutex == NULL)
    return FALSE;

  Data->eventListTail = NULL;
  Data->eventListHead = NULL;
  Data->eventHandler = EventHandler;
  Data->handlerContext = HandlerContext;

  return TRUE;
}



/* exported
 */

enum diotError diotInitialize(void (*EventHandler)(const struct diotEvent*, void*), void* HandlerContext)
{
  /* assume thread safe */

  enum diotError diotError = DIOT_ERROR_FAILURE;
  HANDLE deviceHandle = INVALID_HANDLE_VALUE;
  HANDLE comEvent = NULL;
  BOOL isSuccess;
  DWORD size;

  if (gIsInitialized == TRUE)
    {
      DIOT_DEBUG_ERROR("gIsInitialized == TRUE\n");
      diotError = DIOT_ERROR_ALREADY_INITIALIZED;
      goto onError;
    }

  deviceHandle =
    CreateFileW(L"\\\\.\\" DIOT_DEVICE_NAME,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

  if (deviceHandle == INVALID_HANDLE_VALUE)
    {
      DIOT_DEBUG_ERROR("CreateFileW() == %x\n", GetLastError());
      diotError = DIOT_ERROR_NOT_FOUND;
      goto onError;
    }

  comEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (comEvent == NULL)
    {
      DIOT_DEBUG_ERROR("CreateEvent() == 0x%x\n", GetLastError());
      diotError = DIOT_ERROR_FAILURE;
      goto onError;
    }

  isSuccess =
    DeviceIoControl(deviceHandle,
		    DIOT_IOCTL_SET_USER_EVENT,
		    &comEvent,
		    sizeof(comEvent),
		    NULL,
		    0,
		    &size,
		    NULL);

  if (isSuccess == FALSE)
    {
      DIOT_DEBUG_ERROR("DeviceIoControl(DIOT_IOCTL_SET_COM_EVENT) == 0x%x\n", GetLastError());
      diotError = DIOT_ERROR_FAILURE;
      goto onError;
    }

  /* revert to failure */

  diotError = DIOT_ERROR_FAILURE;

  gDeviceHandle = deviceHandle;
  gComEvent = comEvent;

  if (initProcessorData(&gProcessorThread.data.processorData, EventHandler, HandlerContext) == FALSE)
    {
      DIOT_DEBUG_ERROR("initProcessorData() == FALSE\n");
      goto onError;
    }

  if (createThread(&gProcessorThread, processorEntry) == FALSE)
    {
      DIOT_DEBUG_ERROR("createThread(processor)\n");
      goto onError;
    }

  if (createThread(&gNotifierThread, notifierEntry) == FALSE)
    {
      DIOT_DEBUG_ERROR("createThread(notifier)\n");
      goto onError;
    }

  /* success */

  gIsInitialized = TRUE;

  return DIOT_ERROR_SUCCESS;
  
 onError:

  if (deviceHandle != INVALID_HANDLE_VALUE)
    {
      CloseHandle(deviceHandle);
      gDeviceHandle = INVALID_HANDLE_VALUE;
    }

  if (comEvent != NULL)
    {
      CloseHandle(comEvent);
      gComEvent = NULL;
    }

  destroyThread(&gNotifierThread);

  destroyThread(&gProcessorThread);
  releaseProcessorData(&gProcessorThread.data.processorData);

  return diotError;
}


enum diotError diotCleanup(void)
{
  /* assume thread safe */

  if (gIsInitialized == FALSE)
    return DIOT_ERROR_NOT_INITIALIZED;

  gIsInitialized = FALSE;

  destroyThread(&gNotifierThread);

  destroyThread(&gProcessorThread);
  releaseProcessorData(&gProcessorThread.data.processorData);

  if (gComEvent != NULL)
    {
      gComEvent = NULL;
      CloseHandle(gComEvent);
    }

  if (gDeviceHandle != INVALID_HANDLE_VALUE)
    {
      gDeviceHandle = INVALID_HANDLE_VALUE;
      CloseHandle(gDeviceHandle);
    }

  return DIOT_ERROR_SUCCESS;
}


enum diotError diotSetConf(const struct diotConf* Conf)
{
  BOOL isSuccess;

  isSuccess = doDiotDeviceControl(DIOT_IOCTL_SET_CONF, Conf, sizeof(struct diotConf), NULL, 0);

  if (isSuccess == FALSE)
    return DIOT_ERROR_FAILURE;

  return DIOT_ERROR_SUCCESS;
}


enum diotError diotGetConf(struct diotConf* Conf)
{
  BOOL isSuccess;

  isSuccess = doDiotDeviceControl(DIOT_IOCTL_GET_CONF, NULL, 0, Conf, sizeof(struct diotConf));

  if (isSuccess == FALSE)
    return DIOT_ERROR_FAILURE;

  return DIOT_ERROR_SUCCESS;
}


static enum diotError setRanges(DWORD ControlCode, const struct diotRange* Ranges, unsigned int RangeCount)
{
  BOOL isSuccess;

  isSuccess = doDiotDeviceControl(ControlCode, Ranges, RangeCount * sizeof(Ranges[0]), NULL, 0);
  if (isSuccess == FALSE)
    return DIOT_ERROR_FAILURE;

  return DIOT_ERROR_SUCCESS;
}


static enum diotError getRanges(DWORD ControlCode, struct diotRange** Ranges, unsigned int* RangeCount)
{
  /* todo: rely on an error code to loop */

  PVOID buffer = NULL;
  BOOL isSuccess;
  ULONG count;

  *Ranges = NULL;
  *RangeCount = 0;

#define DIOT_MAX_RANGE_COUNT 32

  for (count = 1; count <= DIOT_MAX_RANGE_COUNT; ++count)
    {
      buffer = malloc(count * sizeof(struct diotRange));
      if (buffer == NULL)
	goto onError;

      isSuccess = doDiotDeviceControl(ControlCode, NULL, 0, buffer, count * sizeof(struct diotRange));
      if (isSuccess == TRUE)
	break;

      free(buffer);
    }

  if (count > DIOT_MAX_RANGE_COUNT)
    goto onError;

  *Ranges = buffer;
  *RangeCount = count;

 onError:

  if (buffer != NULL)
    free(buffer);

  return DIOT_ERROR_FAILURE;
}


enum diotError diotSetMmioRanges(const struct diotRange* Ranges, unsigned int RangeCount)
{
  return setRanges(DIOT_IOCTL_SET_MMIO_RANGES, Ranges, RangeCount);
}


enum diotError diotGetMmioRanges(struct diotRange** Ranges, unsigned int* RangeCount)
{
  return getRanges(DIOT_IOCTL_GET_MMIO_RANGES, Ranges, RangeCount);
}


enum diotError diotSetIoportRanges(const struct diotRange* Ranges, unsigned int RangeCount)
{
  return setRanges(DIOT_IOCTL_SET_IOPORT_RANGES, Ranges, RangeCount);
}


enum diotError diotGetIoportRanges(struct diotRange** Ranges, unsigned int* RangeCount)
{
  return getRanges(DIOT_IOCTL_GET_IOPORT_RANGES, Ranges, RangeCount);
}


enum diotError diotStartTracing(void)
{
  BOOL isSuccess;

  isSuccess = doDiotDeviceControl(DIOT_IOCTL_START_TRACING, NULL, 0, NULL, 0);

  return isSuccess == TRUE ? DIOT_ERROR_SUCCESS : DIOT_ERROR_FAILURE;
}


enum diotError diotStopTracing(void)
{
  BOOL isSuccess;

  isSuccess = doDiotDeviceControl(DIOT_IOCTL_STOP_TRACING, NULL, 0, NULL, 0);

  return isSuccess == TRUE ? DIOT_ERROR_SUCCESS : DIOT_ERROR_FAILURE;
}
