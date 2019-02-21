// R3-Syncronize.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <windows.h>
#include <process.h>
#include <stddef.h>
#include <stdlib.h>
#include <conio.h>

UINT WINAPI	ThreadEvent(LPVOID para)
{
	printf("Enter ThreadEvent!\n");

	HANDLE *pEvent = (HANDLE*)para;
	if (NULL != pEvent)
		SetEvent(*pEvent);

	printf("Leave ThreadEvent!\n");
	return 0;
}

UINT WINAPI	ThreadSemaphere(LPVOID para)
{
	printf("Enter ThreadSemaphere!\n");
	HANDLE *pSemaphore = (HANDLE*)para;

	Sleep(2000);

	ReleaseSemaphore(*pSemaphore, 1, NULL);

	printf("Leave ThreadSemaphere!\n");
	return 0;
}

UINT WINAPI	ThreadMutex1(LPVOID para)
{
	printf("Enter ThreadMutex1\n");
	HANDLE *pMutex = (HANDLE*)para;
	if (pMutex)
	{
		WaitForSingleObject(*pMutex, INFINITE);
		
		WaitForSingleObject(*pMutex, INFINITE);		

		printf("ThreadMutex1 Wait Two Time Mutex\n");

		Sleep(3000);
		printf("ThreadMutex1 Sleep comeback\n");

		ReleaseMutex(*pMutex);
	}
	printf("Leave ThreadMutex1\n");

	return 0L;
}

UINT WINAPI	ThreadMutex2(LPVOID para)
{
	printf("Enter ThreadMutex2\n");
	HANDLE *pMutex = (HANDLE*)para;
	if (pMutex)
	{		
		printf("ThreadMutex2 Wait One Time Mutex Before\n");
		WaitForSingleObject(*pMutex, INFINITE);
		printf("ThreadMutex2 Wait One Time Mutex After\n");

		Sleep(3000);
		printf("ThreadMutex2 Sleep comeback\n");

		ReleaseMutex(*pMutex);
	}
	printf("Leave ThreadMutex2\n");

	return 0L;
}


int _tmain(int argc, _TCHAR* argv[])
{
	//----------------------------------------
	// Event
	HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	HANDLE hThreadEvent = (HANDLE)_beginthreadex(NULL, 0, ThreadEvent, &hEvent, 0, NULL);
	if (hThreadEvent)
		CloseHandle(hThreadEvent);

	WaitForSingleObject(hEvent, INFINITE);

	//----------------------------------------
	// Semaphore
	HANDLE hSemaphore = CreateSemaphore(NULL, 2, 2, NULL);
	if (hSemaphore)
	{
		WaitForSingleObject(hSemaphore, INFINITE);
		WaitForSingleObject(hSemaphore, INFINITE);
		HANDLE hThreadSemaphore = (HANDLE)_beginthreadex(NULL, 0, ThreadSemaphere, &hSemaphore, 0, NULL);
		if (hThreadSemaphore)
			CloseHandle(hThreadSemaphore);
		
		WaitForSingleObject(hSemaphore, INFINITE);
		printf("Main Thread get Semaphore Signal\n");
	}

	//-----------------------------------------
	// Mutex
	HANDLE hMutex = CreateMutex(NULL, FALSE, NULL);
	if (hMutex)
	{
		HANDLE hThreadMutex1 = (HANDLE)_beginthreadex(NULL, 0, ThreadMutex1, &hMutex, 0, NULL);
		HANDLE hThreadMutex2 = (HANDLE)_beginthreadex(NULL, 0, ThreadMutex2, &hMutex, 0, NULL);
		
		HANDLE hThreads[2] = {};
		hThreads[0] = hThreadMutex1;
		hThreads[1] = hThreadMutex2;
		WaitForMultipleObjects(2, hThreads, TRUE, INFINITE);
		CloseHandle(hThreads[0]);
		CloseHandle(hThreads[1]);
	}

	system("pause");

	return 0;
}

