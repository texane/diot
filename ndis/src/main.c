/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 23:00:03 2009 texane
** Last update Mon Apr 20 10:35:42 2009 texane
*/



#include <windows.h>

#pragma warning(push)
#pragma warning(disable: 4201)
#include <ntddndis.h>
#pragma warning(pop)

#include <memory.h>
#include <stdio.h>



/* ndis routines
 */


struct ndisDevice
{
  HANDLE ndisHandle;
};


static BOOLEAN ndisEnumerateDevices(void)
{
  /* HKEY_LOCAL_MACHINE\Software\Microsoft\Windows NT\CurrentVersion\NetworkCards\Nnn
   */

  return FALSE;
}


static BOOLEAN ndisGetGlobalStats(struct ndisDevice* Device,
				  DWORD OidCode,
				  UCHAR* OutputBuffer,
				  DWORD OutputSize)
{
  BOOL isSuccess;
  DWORD returnedSize;
  UCHAR oidData[0x1000];

  isSuccess =
    DeviceIoControl(Device->ndisHandle,
		    IOCTL_NDIS_QUERY_GLOBAL_STATS,
		    &OidCode, sizeof(OidCode),
		    oidData, sizeof(oidData),
		    &returnedSize,
		    NULL);

  if (isSuccess == FALSE)
    return FALSE;

  if (returnedSize > OutputSize)
    returnedSize = OutputSize;

  memcpy(OutputBuffer, oidData, returnedSize);

  return TRUE;
}


static BOOLEAN ndisSetMacAddress(struct ndisDevice* Device, const UCHAR* Address)
{
  UNREFERENCED_PARAMETER(Device);
  UNREFERENCED_PARAMETER(Address);

  return FALSE;
}


static BOOLEAN ndisGetMacAddress(struct ndisDevice* Device, UCHAR* Address)
{
  return ndisGetGlobalStats(Device, OID_802_3_CURRENT_ADDRESS, Address, 6);
}


static BOOLEAN ndisOpenDevice(struct ndisDevice* Device, const char* Name)
{
  HANDLE ndisHandle = INVALID_HANDLE_VALUE;

  ndisHandle =
    CreateFileA(Name,
		FILE_READ_ACCESS | FILE_WRITE_ACCESS,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

  if (ndisHandle == INVALID_HANDLE_VALUE)
    {
      printf("CreateFile() == %u\n", GetLastError());
      goto onError;
    }

  Device->ndisHandle = ndisHandle;

  return TRUE;

 onError:

  return FALSE;
}


static void ndisCloseDevice(struct ndisDevice* Device)
{
  if (Device->ndisHandle == INVALID_HANDLE_VALUE)
    return ;

  CloseHandle(Device->ndisHandle);

  Device->ndisHandle = INVALID_HANDLE_VALUE;
}



/* main
 */

int main(int ac, char** av)
{
  const char* ifName = "\\\\.\\{61FA40D4-427F-439E-8414-A2AB4C27A5C3}";
  struct ndisDevice ndisDevice;
  UCHAR macAddress[6];

  if (ac != 2)
    {
      printf("usage: %s <deviceName>\n", *av);
/*       return -1; */
    }
  else
    {
      ifName = av[1];
    }

  if (ndisOpenDevice(&ndisDevice, ifName) == FALSE)
    return -1;

  if (ndisGetMacAddress(&ndisDevice, macAddress) == FALSE)
    goto onError;

#define MAC_FORMAT_STRING "%02x:%02x:%02x:%02x:%02x:%02x"
#define EXPAND_MAC(M) (M)[0], (M)[1], (M)[2], (M)[3], (M)[4], (M)[5]
  printf(MAC_FORMAT_STRING "\n", EXPAND_MAC(macAddress));

#if 0

  ++macAddress[5];

  if (ndisSetMacAddress(&ndisDevice, macAddress) == FALSE)
    goto onError;

#endif

 onError:

  ndisCloseDevice(&ndisDevice);

  return 0;
}
