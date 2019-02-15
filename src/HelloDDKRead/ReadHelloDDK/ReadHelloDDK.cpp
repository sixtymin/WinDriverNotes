// ReadHelloDDK.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <Windows.h>
#include <stdio.h>

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

	UCHAR buffer[10] = {};
	ULONG ulRead = 0;
	BOOL bRet = ReadFile(hDevice, buffer, 10, &ulRead, NULL);
	if (bRet)
	{
		printf("Read %d byte: ", ulRead);
		for (int i = 0; i < (int)ulRead; i++)
		{
			printf("%02X ", buffer[i]);
		}
		printf("\n");
	}

	CloseHandle(hDevice);

	system("pause");
	return 0;
}