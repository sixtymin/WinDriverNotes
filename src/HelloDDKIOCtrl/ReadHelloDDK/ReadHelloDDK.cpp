// ReadHelloDDK.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <Windows.h>
#include <stdio.h>
#include <process.h>

#define ID_IOCTL_TEST1 CTL_CODE(\
					FILE_DEVICE_UNKNOWN,\
					0x800,\
					METHOD_BUFFERED,\
					FILE_ANY_ACCESS)

#define ID_IOCTL_TRANSMIT_EVENT CTL_CODE(\
					FILE_DEVICE_UNKNOWN,\
					0x801,\
					METHOD_BUFFERED,\
					FILE_ANY_ACCESS)

unsigned int __stdcall EventWaitThread(void * para)
{
	if (NULL == para)
	{
		printf("param is null");
		return 0;
	}

	HANDLE *pHandle = (HANDLE*)para;
	printf("Before Wait For Event!\n");
	WaitForSingleObject(*pHandle, INFINITE);
	printf("Wait For Event Has Signal!\n");

	return 0;
}


int _tmain(int argc, _TCHAR* argv[])
{
	HANDLE hDevice = CreateFileW(L"\\\\.\\HelloDDK",
		GENERIC_READ,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		printf("Failed to obtain file handle to device: %s with Win32 Error code: %d\n",
			"MyDDKDevice", GetLastError());
		return 1;
	}

	DWORD dwReturn = 0;
	CHAR szInput[10] = {};
	CHAR szOutput[10] = {};
	if (DeviceIoControl(hDevice, ID_IOCTL_TEST1, szInput, 5, szOutput, 5, &dwReturn, NULL))
	{
		printf("IO Ctrl ID_IOCTL_TEST1 sucess.\n");
	}	
	else
	{
		printf("IO Ctrl ID_IOCTL_TEST1 Failed.\n");
	}

	// >>>>>>>>>> Pass Handle to Driver
	//HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	//if (hEvent == NULL)
	//{
	//	printf("CreateEvent Error %d", GetLastError());
	//	return 1;
	//}

	//printf("Event Handle: 0x%08X", hEvent);
	//HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, EventWaitThread, &hEvent, 0, NULL);

	//DWORD dwReturn = 0;
	//CHAR szOutput[10] = {};
	//if (DeviceIoControl(hDevice, ID_IOCTL_TRANSMIT_EVENT, &hEvent, 4, szOutput, 5, &dwReturn, NULL))
	//{
	//	printf("IO Ctrl ID_IOCTL_TRANSMIT_EVENT sucess.\n");
	//}	
	//else
	//{
	//	printf("IO Ctrl ID_IOCTL_TRANSMIT_EVENT Failed.\n");
	//}

	//WaitForSingleObject(hThread, INFINITE);


	//DWORD dwReturn = 0;
	//CHAR szInput[10] = {};
	//CHAR szOutput[10] = {};
	//if (DeviceIoControl(hDevice, ID_IOCTL_TRANSMIT_EVENT, szInput, 5, szOutput, 5, &dwReturn, NULL))
	//{
	//	printf("IO Ctrl ID_IOCTL_TEST1 sucess.\n");
	//}	
	//else
	//{
	//	printf("IO Ctrl ID_IOCTL_TEST1 Failed.\n");
	//}	

	//UCHAR buffer[10] = {};
	//ULONG ulRead = 0;
	//BOOL bRet = ReadFile(hDevice, buffer, 10, &ulRead, NULL);
	//if (bRet)
	//{
	//	printf("Read %d byte: ", ulRead);
	//	for (int i = 0; i < (int)ulRead; i++)
	//	{
	//		printf("%02X ", buffer[i]);
	//	}
	//	printf("\n");
	//}

	CloseHandle(hDevice);

	system("pause");
	return 0;
}