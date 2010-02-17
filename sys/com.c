/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 22:54:50 2009 texane
** Last update Thu Apr 16 13:36:37 2009 texane
*/



#include <wdm.h>
#include "com.h"
#include "mmio.h"
#include "ioport.h"
#include "eventQueue.h"
#include "../../common/diotCom.h"
#include "../../common/diotTypes.h"
#include "../../common/diotDebug.h"



/* globals
 */

static UNICODE_STRING gDeviceName = RTL_CONSTANT_STRING(L"\\Device\\" DIOT_DEVICE_NAME);
static UNICODE_STRING gLinkName = RTL_CONSTANT_STRING(L"\\DosDevices\\" DIOT_DEVICE_NAME);
static PDEVICE_OBJECT gDeviceObject = NULL;



/* io helpers
 */

static void completeRequest(PIRP Irp, NTSTATUS Status, ULONG Information)
{
  Irp->IoStatus.Status = Status;
  Irp->IoStatus.Information = Information;

  IoCompleteRequest(Irp, IO_NO_INCREMENT);
}



/* device iocontrol handlers
 */

static NTSTATUS handleSetUserEvent(const void* InputBuffer,
				   ULONG InputSize,
				   PVOID OutputBuffer,
				   ULONG OutputSize,
				   PULONG Information)
{
  /* set the com event handle
     InputBuffer: PHANDLE
     OutputBuffer: unused
   */

  NTSTATUS status = STATUS_UNSUCCESSFUL;
  HANDLE eventHandle;
  PVOID userEvent;

  UNREFERENCED_PARAMETER(OutputBuffer);
  UNREFERENCED_PARAMETER(OutputSize);

  DIOT_DEBUG_ENTER();

  *Information = 0;

  if (InputSize < sizeof(HANDLE))
    {
      DIOT_DEBUG_ERROR("Size < sizeof(HANDLE)\n");
      status = STATUS_BUFFER_TOO_SMALL;
      goto onError;
    }

  eventHandle = *(const HANDLE*)InputBuffer;

  status =
    ObReferenceObjectByHandle(eventHandle,
			      EVENT_ALL_ACCESS,
			      *ExEventObjectType,
			      UserMode,
			      &userEvent,
			      NULL);

  if (status != STATUS_SUCCESS)
    {
      DIOT_DEBUG_ERROR("ObReferenceObjectByHandle() == 0x%08x\n", status);
      goto onError;
    }

  if (eventQueueSetUserEvent(userEvent) == FALSE)
    {
      DIOT_DEBUG_ERROR("eventQueueSetUserEvent() == FALSE\n");
      ObDereferenceObject(userEvent);
      goto onError;
    }

  /* success */

  status = STATUS_SUCCESS;

 onError:

  *Information = 0;

  return status;
}


static NTSTATUS handleGetEvents(const void* InputBuffer,
				ULONG InputSize,
				PVOID OutputBuffer,
				ULONG OutputSize,
				PULONG Information)
{
  /* fetch pending events
     InputBuffer: unused
     OutputBuffer: struct diotEvent*
   */

  UNREFERENCED_PARAMETER(InputBuffer);
  UNREFERENCED_PARAMETER(InputSize);

  eventQueuePop(OutputBuffer, OutputSize, Information);

  return STATUS_SUCCESS;
}


static NTSTATUS handleSetConf(const void* InputBuffer,
			      ULONG InputSize,
			      PVOID OutputBuffer,
			      ULONG OutputSize,
			      PULONG Information)
{
  DIOT_DEBUG_ENTER();

  UNREFERENCED_PARAMETER(InputBuffer);
  UNREFERENCED_PARAMETER(InputSize);
  UNREFERENCED_PARAMETER(OutputBuffer);
  UNREFERENCED_PARAMETER(OutputSize);

  *Information = 0;

  return STATUS_SUCCESS;
}


static NTSTATUS handleGetConf(const void* InputBuffer,
			      ULONG InputSize,
			      PVOID OutputBuffer,
			      ULONG OutputSize,
			      PULONG Information)
{
  DIOT_DEBUG_ENTER();

  UNREFERENCED_PARAMETER(InputBuffer);
  UNREFERENCED_PARAMETER(InputSize);
  UNREFERENCED_PARAMETER(OutputBuffer);
  UNREFERENCED_PARAMETER(OutputSize);

  *Information = 0;

  return STATUS_SUCCESS;
}


static NTSTATUS handleSetMmioRanges(const void* InputBuffer,
				    ULONG InputSize,
				    PVOID OutputBuffer,
				    ULONG OutputSize,
				    PULONG Information)
{
  const ULONG rangeCount = InputSize / sizeof(struct diotRange);

  DIOT_DEBUG_ENTER();

  UNREFERENCED_PARAMETER(OutputBuffer);
  UNREFERENCED_PARAMETER(OutputSize);

  mmioSetRanges(InputBuffer, rangeCount);

  *Information = 0;

  return STATUS_SUCCESS;
}


static NTSTATUS handleGetMmioRanges(const void* InputBuffer,
				    ULONG InputSize,
				    PVOID OutputBuffer,
				    ULONG OutputSize,
				    PULONG Information)
{
  DIOT_DEBUG_ENTER();

  UNREFERENCED_PARAMETER(InputBuffer);
  UNREFERENCED_PARAMETER(InputSize);
  UNREFERENCED_PARAMETER(OutputBuffer);
  UNREFERENCED_PARAMETER(OutputSize);

  *Information = 0;

  return STATUS_SUCCESS;
}


static NTSTATUS handleSetIoportRanges(const void* InputBuffer,
				      ULONG InputSize,
				      PVOID OutputBuffer,
				      ULONG OutputSize,
				      PULONG Information)
{
  const ULONG rangeCount = InputSize / sizeof(struct diotRange);

  DIOT_DEBUG_ENTER();

  UNREFERENCED_PARAMETER(OutputBuffer);
  UNREFERENCED_PARAMETER(OutputSize);

  ioportSetRanges(InputBuffer, rangeCount);

  *Information = 0;

  return STATUS_SUCCESS;
}


static NTSTATUS handleGetIoportRanges(const void* InputBuffer,
				      ULONG InputSize,
				      PVOID OutputBuffer,
				      ULONG OutputSize,
				      PULONG Information)
{
  DIOT_DEBUG_ENTER();

  UNREFERENCED_PARAMETER(InputBuffer);
  UNREFERENCED_PARAMETER(InputSize);
  UNREFERENCED_PARAMETER(OutputBuffer);
  UNREFERENCED_PARAMETER(OutputSize);

  *Information = 0;

  return STATUS_SUCCESS;
}


static NTSTATUS handleStartTracing(const void* InputBuffer,
				   ULONG InputSize,
				   PVOID OutputBuffer,
				   ULONG OutputSize,
				   PULONG Information)
{
  DIOT_DEBUG_ENTER();

  UNREFERENCED_PARAMETER(InputBuffer);
  UNREFERENCED_PARAMETER(InputSize);
  UNREFERENCED_PARAMETER(OutputBuffer);
  UNREFERENCED_PARAMETER(OutputSize);

  *Information = 0;

  return STATUS_SUCCESS;
}


static NTSTATUS handleStopTracing(const void* InputBuffer,
				  ULONG InputSize,
				  PVOID OutputBuffer,
				  ULONG OutputSize,
				  PULONG Information)
{
  DIOT_DEBUG_ENTER();

  UNREFERENCED_PARAMETER(InputBuffer);
  UNREFERENCED_PARAMETER(InputSize);
  UNREFERENCED_PARAMETER(OutputBuffer);
  UNREFERENCED_PARAMETER(OutputSize);

  *Information = 0;

  return STATUS_SUCCESS;
}


static NTSTATUS handleDeviceControl(PIRP Irp, PIO_STACK_LOCATION IrpSp, PULONG Information)
{
  NTSTATUS status = STATUS_UNSUCCESSFUL;
  ULONG information = 0;
  ULONG controlCode = 0;
  void* inputBuffer = NULL;
  ULONG inputSize = 0;
  void* outputBuffer = NULL;
  ULONG outputSize = 0;
  NTSTATUS (*handler)(const void*, ULONG, void*, ULONG, PULONG) = NULL;

  DIOT_DEBUG_ENTER();

  controlCode = IrpSp->Parameters.DeviceIoControl.IoControlCode;

  switch (controlCode)
    {
    case DIOT_IOCTL_SET_USER_EVENT:
      {
	handler = handleSetUserEvent;

	inputBuffer = IrpSp->Parameters.DeviceIoControl.Type3InputBuffer;
	inputSize = IrpSp->Parameters.DeviceIoControl.InputBufferLength;

	break;
      }

    case DIOT_IOCTL_GET_EVENTS:
      {
	handler = handleGetEvents;

	outputBuffer = Irp->UserBuffer;
	outputSize = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

	break;
      }

    case DIOT_IOCTL_SET_CONF:
      {
	handler = handleSetConf;

	inputBuffer = IrpSp->Parameters.DeviceIoControl.Type3InputBuffer;
	inputSize = IrpSp->Parameters.DeviceIoControl.InputBufferLength;

	break;
      }

    case DIOT_IOCTL_GET_CONF:
      {
	handler = handleGetConf;

	outputBuffer = Irp->UserBuffer;
	outputSize = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

	break;
      }

    case DIOT_IOCTL_SET_MMIO_RANGES:
      {
	handler = handleSetMmioRanges;

	inputBuffer = IrpSp->Parameters.DeviceIoControl.Type3InputBuffer;
	inputSize = IrpSp->Parameters.DeviceIoControl.InputBufferLength;

	break;
      }

    case DIOT_IOCTL_GET_MMIO_RANGES:
      {
	handler = handleGetMmioRanges;

	outputBuffer = Irp->UserBuffer;
	outputSize = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

	break;
      }

    case DIOT_IOCTL_SET_IOPORT_RANGES:
      {
	handler = handleSetIoportRanges;

	inputBuffer = IrpSp->Parameters.DeviceIoControl.Type3InputBuffer;
	inputSize = IrpSp->Parameters.DeviceIoControl.InputBufferLength;

	break;
      }

    case DIOT_IOCTL_GET_IOPORT_RANGES:
      {
	handler = handleGetIoportRanges;

	outputBuffer = Irp->UserBuffer;
	outputSize = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

	break;
      }

    case DIOT_IOCTL_START_TRACING:
      {
	handler = handleStartTracing;

	break;
      }

    case DIOT_IOCTL_STOP_TRACING:
      {
	handler = handleStopTracing;

	break;
      }

    default:
      {
	DIOT_DEBUG_ERROR("ControlCode == 0x%x\n", controlCode);
	status = STATUS_NOT_SUPPORTED;
	goto onError;
      }
    }

  __try
    {
      if (inputBuffer != NULL)
	ProbeForRead(inputBuffer, inputSize, 1);

      if (outputBuffer != NULL)
	ProbeForWrite(outputBuffer, outputSize, 1);

      /* handler call has to be inside an exception
	 block in order for invalid accesses to be
	 caught, even when previously probed.
       */

      status =
	handler(inputBuffer, inputSize,
		outputBuffer, outputSize,
		&information);
    }
  __except(EXCEPTION_EXECUTE_HANDLER)
    {
      status = STATUS_INVALID_USER_BUFFER;
      goto onError;
    }

 onError:

  *Information = information;

  return status;
}



/* create handler
 */

static NTSTATUS handleCreate(PIRP Irp, PULONG Information)
{
  PIO_STACK_LOCATION const irpSp = IoGetCurrentIrpStackLocation(Irp);

  DIOT_DEBUG_ENTER();

  *Information = 0;

  if (irpSp->Parameters.Create.ShareAccess)
    {
      DIOT_DEBUG_ERROR("irpSp->Create.ShareAccess\n");
      return STATUS_SHARING_VIOLATION;
    }

  return STATUS_SUCCESS;
}



/* cleanup handler
 */

static NTSTATUS handleCleanup(PIRP Irp, PULONG Information)
{
  PVOID userEvent;

  DIOT_DEBUG_ENTER();

  UNREFERENCED_PARAMETER(Irp);

  userEvent = eventQueueGetUserEvent();
  if (userEvent != NULL)
    ObDereferenceObject(userEvent);

  *Information = 0;

  return STATUS_SUCCESS;
}



/* irp dispatcher
 */

static NTSTATUS dispatchIrp(PDEVICE_OBJECT Device, PIRP Irp)
{
  PIO_STACK_LOCATION irpSp;
  ULONG information;
  NTSTATUS status;

  DIOT_DEBUG_ENTER();

  irpSp = IoGetCurrentIrpStackLocation(Irp);

  switch (irpSp->MajorFunction)
    {
    case IRP_MJ_CREATE:
      status = handleCreate(Irp, &information);
      break;

    case IRP_MJ_CLEANUP:
      status = handleCleanup(Irp, &information);
      break;

    case IRP_MJ_DEVICE_CONTROL:
      status = handleDeviceControl(Irp, irpSp, &information);
      break;

    default:
      DIOT_DEBUG_ERROR("irpSp->MajorFunction == 0x%x\n", irpSp->MajorFunction);
      status = STATUS_NOT_SUPPORTED;
      information = 0;
      break;
    }

  completeRequest(Irp, status, information);

  return status;
}



/* exported
 */

NTSTATUS comInitialize(PDRIVER_OBJECT DriverObject)
{
  NTSTATUS status;
  PDEVICE_OBJECT deviceObject;
  ULONG i;

  for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; ++i)
    DriverObject->MajorFunction[i] = dispatchIrp;

  status =
    IoCreateDevice(DriverObject,
		   0,
		   &gDeviceName,
		   FILE_DEVICE_UNKNOWN,
		   FILE_DEVICE_SECURE_OPEN,
		   TRUE,
		   &deviceObject);

  if ((status != STATUS_SUCCESS) && (status != STATUS_OBJECT_NAME_COLLISION))
    {
      DIOT_DEBUG_ERROR("IoCreateDevice() == 0x%08x\n", status);
      return status;
    }

  status = IoCreateSymbolicLink(&gLinkName, &gDeviceName);

  if (status != STATUS_SUCCESS)
    {
      DIOT_DEBUG_ERROR("IoCreateSymbolicLink() == 0x%08x\n", status);
      IoDeleteDevice(deviceObject);
      return status;
    }

  gDeviceObject = deviceObject;

  deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

  return STATUS_SUCCESS;
}


void comCleanup(void)
{
  /* assume thread safe */

  PVOID userEvent;
  NTSTATUS status;

  DIOT_DEBUG_ENTER();

  userEvent = eventQueueGetUserEvent();
  if (userEvent != NULL)
    ObDereferenceObject(userEvent);

  if (gDeviceObject == NULL)
    goto onError;

  IoDeleteSymbolicLink(&gLinkName);
  IoDeleteDevice(gDeviceObject);
  gDeviceObject = NULL;

 onError:

  return ;
}
