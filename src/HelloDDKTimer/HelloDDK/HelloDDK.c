

#include "HelloDDK.h"

#define TIMER_OUT 3

typedef struct _DEVICE_EXTENSION {
	PDEVICE_OBJECT pDevice;
	UNICODE_STRING ustrDeviceName;	// 设备名称
	UNICODE_STRING ustrSymLinkName;	// 符号链接名
	LONG lTimerCount;

	KDPC pollingDPC;
	KTIMER pollingTimer;
	LARGE_INTEGER pollingInterval;
}DEVICE_EXTENSION, *PDEVICE_EXTENSION;


WCHAR* s_lpDeviceName = L"\\Device\\MyDDKDevice";
WCHAR* s_lpSymbolicName = L"\\??\\HelloDDK";

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

#pragma PAGECODE
NTSTATUS HelloDDKDeviceIOControl(IN PDEVICE_OBJECT pDevObj,
	                             IN PIRP pIrp)
{
	NTSTATUS status = STATUS_SUCCESS;
	KdPrint(("Enter HelloDDKDeviceIOControl\n"));

	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrp);
	//ULONG cbIn = stack->Parameters.DeviceIoControl.InputBufferLength;
	//ULONG cbout = stack->Parameters.DeviceIoControl.OutputBufferLength;
	ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;

	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	//ULONG info = 0;
	switch (code)
	{
	case IOCTRL_START_TIME:
	{
		KdPrint(("IOCTRL_START_TIMER\n"));
		pDevExt->lTimerCount = TIMER_OUT;
		IoStartTimer(pDevObj);
		break;
	}
	case IOCTRL_STOP_TIME:
	{
		KdPrint(("IOCTRL_STOP_TIME"));
		IoStopTimer(pDevObj);
		break;
	}
	case IOCTRL_START_DPC_TIME:
	{
		KdPrint(("IOCTRL_START_DPC_TIME\n"));
		ULONG ulMicroSeconds = *(PULONG)pIrp->AssociatedIrp.SystemBuffer;
		pDevExt->pollingInterval = RtlConvertLongToLargeInteger(ulMicroSeconds * -10);
		KeSetTimer(&pDevExt->pollingTimer, pDevExt->pollingInterval, &pDevExt->pollingDPC);
		break;
		break;
	}
	case IOCTRL_STOP_DPC_TIME:
	{
		KdPrint(("IOCTRL_STOP_DPC_TIME"));
		KeCancelTimer(&pDevExt->pollingTimer);
		break;
	}
	default:
		break;
	}

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	KdPrint(("Leave HelloDDKDeviceIOControl\n"));

	return status;
}

#pragma LOCKEDCODE
VOID OnTimer(IN PDEVICE_OBJECT pDevObj, IN PVOID Context)
{
	UNREFERENCED_PARAMETER(Context);
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	KdPrint(("Enter OnTimer\n"));
	InterlockedDecrement(&pDevExt->lTimerCount);
	LONG preCount = InterlockedCompareExchange(&pDevExt->lTimerCount, TIMER_OUT, 0);
	if (preCount == 0)
	{
		KdPrint(("%d seconds is time out!\n", TIMER_OUT));
	}

	PEPROCESS pEProcess = IoGetCurrentProcess();
	PTSTR procName = (PTSTR)((ULONG)pEProcess + 0x174);
	KdPrint(("The current process is %s\n", procName));
	return;
}

#pragma LOCKEDCODE
VOID PoolingTimeProc(IN PKDPC pDpc, IN PVOID pContext, IN PVOID SysArg1, IN PVOID SysArg2)
{
	UNREFERENCED_PARAMETER(pDpc);
	UNREFERENCED_PARAMETER(SysArg1);
	UNREFERENCED_PARAMETER(SysArg2);
	PDEVICE_OBJECT pDevObj = (PDEVICE_OBJECT)pContext;
	if (NULL != pDevObj)
	{
		PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;
		KeSetTimer(&pDevExt->pollingTimer, pDevExt->pollingInterval, &pDevExt->pollingDPC);
		KdPrint(("PollingTimerDPC\n"));

		PEPROCESS pEProcess = IoGetCurrentProcess();
		PTSTR procName = (PTSTR)((ULONG)pEProcess + 0x174);
		KdPrint(("The current process is %s\n", procName));
	}

	return ;
}

#pragma PAGECODE
VOID Time_Test()
{
	LARGE_INTEGER currSysTime;
	KeQuerySystemTime(&currSysTime);

	LARGE_INTEGER curLocalTime;
	ExSystemTimeToLocalTime(&currSysTime, &curLocalTime);

	TIME_FIELDS curTimeInfo;
	RtlTimeToTimeFields(&curLocalTime, &curTimeInfo);
	KdPrint(("Current Year: %d\n", curTimeInfo.Year));
	KdPrint(("Current Month: %d\n", curTimeInfo.Month));
	KdPrint(("Current Day: %d\n", curTimeInfo.Day));
	KdPrint(("Current Hour: %d\n", curTimeInfo.Hour));
	KdPrint(("Current Minute: %d\n", curTimeInfo.Minute));
	KdPrint(("Current Second: %d\n", curTimeInfo.Second));
	KdPrint(("Current Milliseconds: %d\n", curTimeInfo.Milliseconds));
	KdPrint(("Current WeekDay: %d\n", curTimeInfo.Weekday));
	return;
}

#pragma INITCODE 
NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath)
{
	NTSTATUS status;	
	KdPrint(("Enter DriverEntry %wZ\n", pRegistryPath));

	pDriverObject->DriverUnload = HelloDDKUnload;
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = HelloDDKDispatchRoutine;
	pDriverObject->MajorFunction[IRP_MJ_READ] = HelloDDKDispatchRoutine;
	pDriverObject->MajorFunction[IRP_MJ_WRITE] = HelloDDKDispatchRoutine;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = HelloDDKDispatchRoutine;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = HelloDDKDeviceIOControl;

	status = CreateDevice(pDriverObject);

	Time_Test();

	KdPrint(("Leave DriverEntry\n"));
	return status;
}

#pragma INITCODE
NTSTATUS CreateDevice(IN PDRIVER_OBJECT pDriverObject)
{
	NTSTATUS status;
	PDEVICE_OBJECT pDevObj;
	PDEVICE_EXTENSION pDevExt;

	// 创建设备名
	UNICODE_STRING devName;
	RtlInitUnicodeString(&devName, s_lpDeviceName);
	status = IoCreateDevice(pDriverObject,			// 创建设备
		sizeof(DEVICE_EXTENSION),
		&devName,
		FILE_DEVICE_UNKNOWN,
		0, TRUE,
		&pDevObj);
	if (!NT_SUCCESS(status))
		return status;

	pDevObj->Flags |= DO_BUFFERED_IO;
	pDevExt = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	pDevExt->pDevice = pDevObj;
	pDevExt->ustrDeviceName = devName;

	// 符号链接
	UNICODE_STRING symLinkName;
	RtlInitUnicodeString(&symLinkName, s_lpSymbolicName);
	pDevExt->ustrSymLinkName = symLinkName;
	status = IoCreateSymbolicLink(&symLinkName, &devName);
	if (!NT_SUCCESS(status))
	{
		IoDeleteDevice(pDevObj);
		return status;
	}

	// 初始化I/O定时器
	IoInitializeTimer(pDevObj, OnTimer, NULL);

	// 初始化DPC定时器
	KeInitializeTimer(&pDevExt->pollingTimer);
	KeInitializeDpc(&pDevExt->pollingDPC, PoolingTimeProc, (PVOID)pDevObj);


	return STATUS_SUCCESS;
}

#pragma PAGECODE
VOID HelloDDKUnload(IN PDRIVER_OBJECT pDriverObject)
{
	PDEVICE_OBJECT pNextObj;
	KdPrint(("Enter DriverUnload\n"));
	pNextObj = pDriverObject->DeviceObject;
	while (pNextObj != NULL)
	{
		PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)pNextObj->DeviceExtension;

		// 删除符号
		UNICODE_STRING pLinkName = pDevExt->ustrSymLinkName;
		IoDeleteSymbolicLink(&pLinkName);
		pNextObj = pNextObj->NextDevice;
		IoDeleteDevice(pDevExt->pDevice);
	}
	KdPrint(("Leave DriverUnload\n"));
}

#pragma PAGECODE
NTSTATUS HelloDDKDispatchRoutine(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
	KdPrint(("Enter HelloDDKDispatchRoutine DevObj: %p\n", pDevObj));
	NTSTATUS status = STATUS_SUCCESS;



	// 完成IRP
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	KdPrint(("Leave HelloDDKDispatchRoutine\n"));
	return status;
}

