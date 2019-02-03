
#include <ntddk.h>

UINT16 DayOfMon[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
#define SENCOND_OF_DAY 86400
//ULONG DeadTime = 1548431212;
ULONG DeadTime = 1551109612;

extern POBJECT_TYPE *PsThreadType;

PETHREAD pThreadObj = NULL;
BOOLEAN TimeSwitch = FALSE;

NTSTATUS Unload(PDRIVER_OBJECT driver)
{
	KdPrint(("Unload nothing to do\n"));
	TimeSwitch = TRUE;
	KeWaitForSingleObject(pThreadObj, Executive, KernelMode, FALSE, NULL);
	ObDereferenceObject(pThreadObj);
	return STATUS_SUCCESS;
}

BOOLEAN CheckTimeLocal()
{
	LARGE_INTEGER snow, now, tickcount;
	TIME_FIELDS now_fiedls;

	KeQuerySystemTime(&snow);	// 获取标准时间，格林尼治时间
	ExSystemTimeToLocalTime(&snow, &now); // 转化为北京时间
	RtlTimeToTimeFields(&now, &now_fiedls); // 转化为结构体

	KdPrint(("%d-%d-%d--\n", now_fiedls.Year, now_fiedls.Month, now_fiedls.Day));

	UINT16 iYear, iMon, iDay, iHour, iMin, iSec;
	iYear = now_fiedls.Year;
	iMon = now_fiedls.Month;
	iDay = now_fiedls.Day;
	iHour = now_fiedls.Hour;
	iMin = now_fiedls.Minute;
	iSec = now_fiedls.Second;

	SHORT i, Cyear = 0;
	ULONG CountDay = 0;
	for (i = 1970; i < iYear; i++)
	{
		if (((i % 4 == 0) && (i % 100 != 0)) || (i % 400 == 0)) Cyear++;
	}

	CountDay = Cyear * 364 + (iYear - 1970 - Cyear) * 365;
	for (i = 1; i < iMon; i++)
	{
		if ((i == 2) && (((i % 4 == 0) && (i % 100 != 0)) || (i % 400 == 0)))
		{
			CountDay += 29;
		}
		else
		{
			CountDay += DayOfMon[i-1];
		}
	}
	CountDay += (iDay - 1);
	CountDay = CountDay * SENCOND_OF_DAY + (unsigned long)iHour * 3600 + (unsigned long)iMin * 60 + iSec;

	KdPrint(("Now: %d \n", CountDay));
	if (CountDay < DeadTime)
	{
		return TRUE;
	}

	return FALSE;
}

VOID CheckTimeThread(PVOID StartContext)
{
	LARGE_INTEGER SleepTime;
	SleepTime.QuadPart = -200000000;

	KdPrint(("Enter the thread\n"));
	while (1)
	{
		if (TimeSwitch)
			break;

		if (!CheckTimeLocal())
		{
			KdPrint(("Driver Invalid\n"));
		}

		KeDelayExecutionThread(KernelMode, FALSE, &SleepTime);
	}
	PsTerminateSystemThread(STATUS_SUCCESS);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
	KdPrint(("Enter the driver\n"));
	driver->DriverUnload = Unload;
	
	if (!CheckTimeLocal())
	{
		return STATUS_NOT_SUPPORTED;  // 驱动自卸载
	}

	OBJECT_ATTRIBUTES ObjAttr = {0};
	HANDLE ThreadHandle = 0;
	InitializeObjectAttributes(&ObjAttr, NULL, OBJ_KERNEL_HANDLE, 0, NULL);
	NTSTATUS status = PsCreateSystemThread(&ThreadHandle, THREAD_ALL_ACCESS, &ObjAttr, NULL, NULL, CheckTimeThread, NULL);
	if (!NT_SUCCESS(status))
	{
		return STATUS_NOT_SUPPORTED;
	}

	status = ObReferenceObjectByHandle(ThreadHandle, THREAD_ALL_ACCESS, *PsThreadType, KernelMode, &pThreadObj, NULL);
	if (!NT_SUCCESS(status))
	{
		ZwClose(ThreadHandle);
		return STATUS_NOT_SUPPORTED;
	}

	ZwClose(ThreadHandle);

	{
		KdPrint(("Driver Start Work\n"));
	}

	return STATUS_SUCCESS;
}