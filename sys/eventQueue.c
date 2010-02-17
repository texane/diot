/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 22:56:56 2009 texane
** Last update Wed Apr 15 22:56:58 2009 texane
*/



#include <wdm.h>
#include "eventQueue.h"
#include "../common/diotDebug.h"
#include "../common/diotTypes.h"



/* queue item
 */

struct queueItem
{
  struct queueItem* next;
  struct queueItem* prev;
  struct diotEvent event;
};



/* queue descriptor
 */

struct eventQueue
{
  KSPIN_LOCK lock;
  struct queueItem* tail;
  struct queueItem* head;
  SIZE_T size;
};


/* globals
 */

static volatile BOOLEAN isThreadDone = FALSE;
static PVOID threadObject = NULL;
static KEVENT threadEvent;

static struct eventQueue pushedEventsQueue;
static struct eventQueue pendingEventsQueue;

static __declspec(align(4)) volatile PVOID userEvent = NULL;



/* queue routines
 */

static void lockQueue(struct eventQueue* Queue, PKIRQL Irql)
{
  KeAcquireSpinLock(&Queue->lock, Irql);
}


static void unlockQueue(struct eventQueue* Queue, KIRQL Irql)
{
  KeReleaseSpinLock(&Queue->lock, Irql);
}


static void initQueue(struct eventQueue* Queue)
{
  KeInitializeSpinLock(&Queue->lock);

  Queue->tail = NULL;
  Queue->head = NULL;

  Queue->size = 0;
}


static void destroyQueue(struct eventQueue* Queue)
{
  struct queueItem* head;
  struct queueItem* prev;
  KIRQL irql;

  lockQueue(Queue, &irql);

  head = Queue->head;

  Queue->tail = NULL;
  Queue->head = NULL;
  Queue->size = 0;

  unlockQueue(Queue, irql);

  while (head != NULL)
    {
      prev = head;
      head = head->next;
      ExFreePoolWithTag(prev, DIOT_POOL_TAG);
    }
}


/* signal the user event
 */

static void signalUserEvent(void)
{
  PVOID const event = eventQueueGetUserEvent();

  if (event == NULL)
    return ;

  KeSetEvent(event, 0, FALSE);

  if (eventQueueSetUserEvent(event) == FALSE)
    ObDereferenceObject(event);
}



/* thread
 */

static void threadEntry(PVOID Params)
{
  NTSTATUS status;
  struct queueItem* head;
  struct queueItem* tail;
  SIZE_T size;
  KIRQL irql;

  while (isThreadDone == FALSE)
    {
      status = KeWaitForSingleObject(&threadEvent, Executive, KernelMode, FALSE, NULL);

      if (status != STATUS_SUCCESS)
	break;

      if (isThreadDone == TRUE)
	break;

      /* merge queues */

      lockQueue(&pushedEventsQueue, &irql);

      head = pushedEventsQueue.head;
      tail = pushedEventsQueue.tail;
      size = pushedEventsQueue.size;

      pushedEventsQueue.head = NULL;
      pushedEventsQueue.tail = NULL;
      pushedEventsQueue.size = 0;

      unlockQueue(&pushedEventsQueue, irql);

      if (head != NULL)
	{
	  lockQueue(&pendingEventsQueue, &irql);

	  head->prev = pendingEventsQueue.tail;

	  if (pendingEventsQueue.head == NULL)
	    pendingEventsQueue.head = head;
	  else
	    pendingEventsQueue.tail->next = head;

	  pendingEventsQueue.tail = tail;

	  pendingEventsQueue.size += size;

	  unlockQueue(&pendingEventsQueue, irql);
	}

      KeClearEvent(&threadEvent);

      /* signal the userland */

      signalUserEvent();
    }
}



/* exported
 */

NTSTATUS eventQueueInitialize(void)
{
  NTSTATUS status = STATUS_UNSUCCESSFUL;
  HANDLE threadHandle = NULL;

  initQueue(&pushedEventsQueue);
  initQueue(&pendingEventsQueue);

  KeInitializeEvent(&threadEvent, NotificationEvent, FALSE);

  status =
    PsCreateSystemThread(&threadHandle,
			 THREAD_ALL_ACCESS,
			 NULL, NULL, NULL,
			 threadEntry, NULL);

  if (status != STATUS_SUCCESS)
    goto onError;

  status =
    ObReferenceObjectByHandle(threadHandle,
			      THREAD_ALL_ACCESS,
			      *PsThreadType,
			      KernelMode,
			      &threadObject,
			      NULL);

  if (status != STATUS_SUCCESS)
    goto onError;

  status = STATUS_SUCCESS;

 onError:

  if (threadHandle != NULL)
    ZwClose(threadHandle);

  return status;
}


void eventQueueCleanup(void)
{
  PVOID event;

  event = eventQueueGetUserEvent();
  if (event != NULL)
    ObDereferenceObject(event);

  isThreadDone = TRUE;
  KeSetEvent(&threadEvent, 0, TRUE);
  KeWaitForSingleObject(threadObject, Executive, KernelMode, FALSE, NULL);
  ObDereferenceObject(threadObject);

  destroyQueue(&pushedEventsQueue);
  destroyQueue(&pendingEventsQueue);
}


BOOLEAN eventQueueSetUserEvent(PVOID UserEvent)
{
  /* UserEvent a referenced event object
   */

  PVOID event;
  
  event = InterlockedCompareExchangePointer(&userEvent, UserEvent, NULL);
  if (event != NULL)
    {
      DIOT_DEBUG_ERROR("event != NULL\n");
      return FALSE;
    }

  return TRUE;
}


PVOID eventQueueGetUserEvent(void)
{
  return InterlockedExchangePointer(&userEvent, NULL);
}


NTSTATUS eventQueueAlloc(struct diotEvent** Event, enum diotEventType Type, SIZE_T Size)
{
  struct queueItem* item = NULL;
  NTSTATUS status = STATUS_UNSUCCESSFUL;

  if (!Size)
    Size = sizeof(struct diotEvent);

  item = ExAllocatePoolWithTag(NonPagedPool, sizeof(struct queueItem), DIOT_POOL_TAG);
  if (item == NULL)
    return STATUS_INSUFFICIENT_RESOURCES;

  item->next = NULL;
  item->prev = NULL;

  *Event = &item->event;

  (*Event)->type = Type;
  (*Event)->size = Size;
  (*Event)->timestamp.QuadPart = 0LL;

  return STATUS_SUCCESS;
}


void eventQueueFree(struct diotEvent* Event)
{
  struct queueItem* const item = CONTAINING_RECORD(Event, struct queueItem, event);

  ExFreePoolWithTag(item, DIOT_POOL_TAG);
}


void eventQueuePush(struct diotEvent* Event)
{
  /* event allocated by eventQueueAloc
     push back into pushed event queue
   */

  struct queueItem* const item = CONTAINING_RECORD(Event, struct queueItem, event);
  KIRQL irql;

  item->next = NULL;

  lockQueue(&pushedEventsQueue, &irql);

  item->prev = pushedEventsQueue.tail;

  if (pushedEventsQueue.head == NULL)
    pushedEventsQueue.head = item;
  else
    pushedEventsQueue.tail->next = item;

  pushedEventsQueue.tail = item;

  pushedEventsQueue.size += item->event.size;

  unlockQueue(&pushedEventsQueue, irql);

  /* tell the thread about new events */

  KeSetEvent(&threadEvent, 0, FALSE);
}


void eventQueuePop(PUCHAR Buffer, SIZE_T BufferSize, SIZE_T* ActualSize)
{
  /* get from pending events
   */

  struct queueItem* pos;
  struct queueItem* prev;
  struct queueItem* head;
  SIZE_T size;
  KIRQL irql;

  lockQueue(&pendingEventsQueue, &irql);

  head = pendingEventsQueue.head;

  if (pendingEventsQueue.size <= BufferSize)
    {
      size = pendingEventsQueue.size;

      pendingEventsQueue.head = NULL;
      pendingEventsQueue.tail = NULL;
      pendingEventsQueue.size = 0;
    }
  else
    {
      size = 0;
      prev = NULL;
      pos = pendingEventsQueue.head;

      while (pos != NULL)
	{
	  if (size + pos->event.size > BufferSize)
	    break;

	  size += pos->event.size;

	  prev = pos;
	  pos = pos->next;
	}

      if (prev != NULL)
	{
	  /* unlink from head to prev */

	  pendingEventsQueue.head = pos;

	  if (pos == NULL)
	    pendingEventsQueue.tail = NULL;
	  else
	    pos->prev = NULL;

	  pendingEventsQueue.size -= size;

	  /* mark end of list */

	  prev->next = NULL;
	}
    }

  unlockQueue(&pendingEventsQueue, irql);

  /* recopy into buffer */

  pos = head;

  while (pos != NULL)
    {
      prev = pos;
      pos = pos->next;

      memcpy(Buffer, &prev->event, prev->event.size);

      Buffer += prev->event.size;

      ExFreePoolWithTag(prev, DIOT_POOL_TAG);
    }

  *ActualSize = size;
}


#if 1

static volatile BOOLEAN isUnitDone = FALSE;
static KEVENT unitThreadEvent;
static PVOID unitThreadObject = NULL;


static void makeDiotMmioInsnEvent(struct diotEvent* Event)
{
  struct diotInsn* const insn = &Event->data.insnEvent.insn;

  memset(&Event->data.insnEvent.mapping, 0, sizeof(struct diotMapping));

  insn->insnSize = 0;
  insn->insnType = DIOT_INSN_TYPE_REG_TO_MEM;
  insn->operandSize = 32;
  insn->srcAddress = 0x2a2a2a2a;
  insn->dstAddress = 0x2a2a2a2a;
  insn->operandValue = 0x2a2a2a2a;
}


static void unitThreadEntry(PVOID Params)
{
  NTSTATUS status;
  struct diotEvent* event;
  LARGE_INTEGER timeout;

  UNREFERENCED_PARAMETER(Params);

  while (isUnitDone == FALSE)
    {
      timeout.QuadPart = -1000000LL;
      status = KeWaitForSingleObject(&unitThreadEvent, Executive, KernelMode, FALSE, &timeout);

      if (status == STATUS_TIMEOUT)
	{
	  status = eventQueueAlloc(&event, DIOT_EVENT_MMIO_INSN, 0);
	  if (status != STATUS_SUCCESS)
	    continue;

	  makeDiotMmioInsnEvent(event);

	  eventQueuePush(event);
	}
      else if (status == STATUS_SUCCESS)
	{
	  if (isUnitDone == TRUE)
	    break;
	}
      else
	{
	  break;
	}
    }
}


void eventQueueStartUnit(void)
{
  NTSTATUS status;
  HANDLE threadHandle = NULL;

  KeInitializeEvent(&unitThreadEvent, SynchronizationEvent, FALSE);

  status =
    PsCreateSystemThread(&threadHandle,
			 THREAD_ALL_ACCESS,
			 NULL, NULL, NULL,
			 unitThreadEntry, NULL);

  if (status != STATUS_SUCCESS)
    goto onError;

  status =
    ObReferenceObjectByHandle(threadHandle,
			      THREAD_ALL_ACCESS,
			      *PsThreadType,
			      KernelMode,
			      &unitThreadObject,
			      NULL);

 onError:

  if (threadHandle != NULL)
    ZwClose(threadHandle);

  return ;
}


void eventQueueStopUnit(void)
{
  isUnitDone = TRUE;
  KeSetEvent(&unitThreadEvent, 0, TRUE);
  KeWaitForSingleObject(unitThreadObject, Executive, KernelMode, FALSE, NULL);
  ObDereferenceObject(unitThreadObject);
}

#endif
