// ReadHelloDDK.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <Windows.h>
#include <stdio.h>
#include <process.h>

VOID CALLBACK MyReadFileCompleteRoutine(DWORD dwErrorCode,
									   DWORD dwNumberOfByteTransferd,
									   LPOVERLAPPED lpOverlapped)
{
	printf("IO Operation end!\n");

	return ;
}

int _tmain(int argc, _TCHAR* argv[])
{
	HANDLE hDevice = CreateFileW(L"\\\\.\\HelloDDK",
		GENERIC_READ,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,	// 设置异步操作标记
		NULL);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		printf("Failed to obtain file handle to device: %s with Win32 Error code: %d\n",
			"MyDDKDevice", GetLastError());
		return 1;
	}
	
	UCHAR buffer[20] = {0};
	OVERLAPPED overlap = {};
	ReadFileEx(hDevice, buffer, 10, &overlap, MyReadFileCompleteRoutine);

	SleepEx(0, TRUE);

	printf("After SleepEx()\n");

	CloseHandle(hDevice);

	system("pause");
	return 0;
}