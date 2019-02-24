
# 定时器 #

### 定时器两种实现 ###

在驱动中定时器有两种方法实现，一种是使用`I/O`定时器例程，另一种是DPC例程。`I/O`定时器是DDK提供的一种定时器，每间隔1秒系统会调用一次`I/O`定时器例程，所以用该方式只适用于整秒的情况。如果要实现毫秒或微秒级别的间隔，则需要使用DPC定时器。

`IoInitializeTimer()`函数用于初始化定时器，`IoStartTimer()`用于启动定时器，`IoStopTimer()`用于停止定时器，如下为定时器例程的形式:

```
VOID OnTimer(IN PDEVICE_OBJECT DeviceObject, IN PVOID Context);
```

> 注意一点，定时器的函数运行在`DISPATCH_LEVEL`级别，所以在其中用到的内存都要是非分页内存，否则可能引起错误。

DPC定时器比`I/O`定时器更加灵活，可以实现任意间隔的定时，DPC定时器内部使用定时器对象`KTIMER`，对定时器设置一个时间间隔后，每隔这段时间操作系统会将DPC例程插入DPC队列中，当DPC被执行时，DPC例程就会别调用。

`KeInitializeTimer()`函数用于初始化定时器对象，`KeInitializeDpc()`用于初始化DPC，`KeSetTimer()`用于开启定时器，`KeCancelTimer()`用于取消定时器。其中在`KeSetTimer`函数中的DueTime为定时时间，如果该参数值为整数，代表绝对时间，即从1601年1月1日到触发DPC的时刻的时间值，单位为100ns；如果参数值为负数，则意味着时间间隔。

如下代码为设置`I/O`定时器和DPC定时器的示例代码：

```
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
```

> 这里需要注意 DPC定时器设置时间间隔时，数值要设置为负数，其次是单位是100ns，设置为1s则需要设置为`10*1000000`。

对应的应用程序就很简单了，这里不再给出，可以参考之前的操作驱动的代码。

### 延迟函数 ###

与定时器相关的是另外一种时间限制方法，等待。在DDK中一共有四种等待的方法，如下所示：

第一种是将等待事件关联一个超时，这样就能等待固定时间。

```
#pragma PAGECODE
VOID WaitMicroSenconds(ULONG ulMicroSeconds)
{
	KEVENT kEvent;
	KeInitializeEvent(&kEvent, SynchronizationEvent, FALSE);
	LARGE_INTEGER timeOut = RtlConvertUlongToLargeInteger(-10 * ulMicroSeconds);
	KeWaitForSingleObject(&kEvent, Executive, KernelMode, FALSE, &timeOut);
	return;
}
```

第二种方法是调用`KeDelayExecutionThread`函数，它可以强制当前线程进入休眠，经过一段时间后恢复继续执行。

```
#pragma PAGECODE
VOID WaitMicroSenconds(ULONG ulMicroSeconds)
{
	LARGE_INTEGER timeOut = RtlConvertUlongToLargeInteger(-10 * ulMicroSeconds);
	KeDelayExecutionThread(KernelMode, FALSE, &timeOut);
	return;
}
```

第三种是使用`KeStallExecutionProcessor()`函数，它不是将线程休眠，而是让CPU处于忙等状态，这种比较耗费CPU资源。这种操作一般不适宜长时间的等待，最好不要等待超过50微妙。

最后一种是定时器的一个变种，用不关联DPC的定时器来等待定时器信号到达。

```
#pragma PAGECODE
VOID WaitMicroSenconds(ULONG ulMicroSeconds)
{
	KTIMER kTimer;
	KeInitializeTimer(&kTimer);
	LARGE_INTEGER timeOut = RtlConvertUlongToLargeInteger(-10 * ulMicroSeconds);
    KeSetTimer(&kTimer， timeOut， NULL);
	KeWaitForSingleObject(&kTimer, Executive, KernelMode, FALSE, NULL);
	return;
}
```

### 时间相关的内核函数 ###

`KeQuerySystemTime()`函数用于返回当前系统时间，以格林尼治时间计时；`ExSystemTimeToLocalTime()`函数将系统时间转换为当前时区对应的时间，`ExLocalTimeToSystemTime()`将当前时间转换为系统的格林尼治时间；`RtlTimeFieldToTime()`可以将当前的年月日得到系统，`RtlTimeToTimeFields()`用于将当前时间转换为具体的年月日。

```
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
```


By Andy@2019-02-24 15:26:59
