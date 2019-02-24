// ReadHelloDDK.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <Windows.h>
#include <stdio.h>
#include <process.h>

#define IOCTRL_START_TIME CTL_CODE(\
						FILE_DEVICE_UNKNOWN,\
						0x800, \
						METHOD_BUFFERED, \
						FILE_ANY_ACCESS)

#define IOCTRL_STOP_TIME CTL_CODE(\
						FILE_DEVICE_UNKNOWN,\
						0x801, \
						METHOD_BUFFERED, \
						FILE_ANY_ACCESS)

#define IOCTRL_START_DPC_TIME CTL_CODE(\
						FILE_DEVICE_UNKNOWN,\
						0x802, \
						METHOD_BUFFERED, \
						FILE_ANY_ACCESS)

#define IOCTRL_STOP_DPC_TIME CTL_CODE(\
						FILE_DEVICE_UNKNOWN,\
						0x803, \
						METHOD_BUFFERED, \
						FILE_ANY_ACCESS)

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
	if (DeviceIoControl(hDevice, IOCTRL_START_TIME, szInput, 5, szOutput, 5, &dwReturn, NULL))
	{
		printf("IO Ctrl IOCTRL_START_TIME sucess.\n");
	}	
	else
	{
		printf("IO Ctrl IOCTRL_START_TIME Failed.\n");
	}

	Sleep(6000);

	if (DeviceIoControl(hDevice, IOCTRL_STOP_TIME, szInput, 5, szOutput, 5, &dwReturn, NULL))
	{
		printf("IO Ctrl IOCTRL_STOP_TIME sucess.\n");
	}	
	else
	{
		printf("IO Ctrl IOCTRL_STOP_TIME Failed.\n");
	}

	DWORD dwMicroSeconds = 1000000;
	if (DeviceIoControl(hDevice, IOCTRL_START_DPC_TIME, &dwMicroSeconds, 4, szOutput, 5, &dwReturn, NULL))
	{
		printf("IO Ctrl IOCTRL_START_DPC_TIME sucess.\n");
	}	
	else
	{
		printf("IO Ctrl IOCTRL_START_DPC_TIME Failed.\n");
	}

	Sleep(10000);

	if (DeviceIoControl(hDevice, IOCTRL_STOP_DPC_TIME, szInput, 5, szOutput, 5, &dwReturn, NULL))
	{
		printf("IO Ctrl IOCTRL_STOP_DPC_TIME sucess.\n");
	}	
	else
	{
		printf("IO Ctrl IOCTRL_STOP_DPC_TIME Failed.\n");
	}

	CloseHandle(hDevice);

	system("pause");
	return 0;
}