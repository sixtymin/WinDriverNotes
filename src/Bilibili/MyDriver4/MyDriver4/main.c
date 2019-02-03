
#include <ntddk.h>

#define IOCTL_TEST1 CTL_CODE(\
			FILE_DEVICE_UNKNOWN,\
			0x800,\
			METHOD_BUFFERED,\
			FILE_ANY_ACCESS)

#define DEVICE_NAME   L"\\Device\\MyReadDevice"
#define SIM_LINK_NAME L"\\??\\MyRead"


PVOID testPool = NULL;
PVOID noneTestpool = NULL;

NTSTATUS Unload(PDRIVER_OBJECT driver)
{
	KdPrint(("this driver is unloading: 0x%08x...\n", driver));
	UNICODE_STRING SymLinkName = RTL_CONSTANT_STRING(SIM_LINK_NAME);
	IoDeleteSymbolicLink(&SymLinkName);

	PDEVICE_OBJECT pDevice = driver->DeviceObject;	
	while (pDevice)
	{
		PDEVICE_OBJECT pDelDevice = pDevice;
		pDevice = pDevice->NextDevice;
		
		IoDeleteDevice(pDelDevice);
	}

	return STATUS_SUCCESS;
}

NTSTATUS DispatchFunction(PDEVICE_OBJECT pDevice, PIRP pIrp)
{
	UNREFERENCED_PARAMETER(pDevice);

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

BOOLEAN MyKillProcess(LONG pid)
{
	HANDLE ProcessHandle;
	NTSTATUS status;
	OBJECT_ATTRIBUTES ObjectAttributes;
	CLIENT_ID myCid;

	InitializeObjectAttributes(&ObjectAttributes, 0, 0, 0, 0);

	myCid.UniqueProcess = (HANDLE)pid;
	myCid.UniqueThread = 0;

	status = ZwOpenProcess(
		&ProcessHandle,
		PROCESS_ALL_ACCESS,
		&ObjectAttributes,
		&myCid);
	if (NT_SUCCESS(status))
	{
		ZwTerminateProcess(ProcessHandle, status);
		ZwClose(ProcessHandle);
		return TRUE;
	}

	return FALSE;
}

NTSTATUS MyDriverIoctl(PDEVICE_OBJECT pDevice, PIRP pIrp)
{
	UNREFERENCED_PARAMETER(pDevice);
	NTSTATUS status = STATUS_SUCCESS;
	PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(pIrp);
	
	ULONG info = 0;
	//ULONG inLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
	ULONG outLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

	ULONG uCode = irpStack->Parameters.DeviceIoControl.IoControlCode;
	switch (uCode)
	{
		case IOCTL_TEST1:
		{
			KdPrint(("self defined IO\n"));
			/*PUCHAR inBuffer = pIrp->AssociatedIrp.SystemBuffer;
			for (ULONG i = 0; i < inLength; i++)
			{
				KdPrint(("%02x", inBuffer[i]));
			}*/
			LONG myPid = *(PLONG)pIrp->AssociatedIrp.SystemBuffer;
			KdPrint(("Input Pid: %d\n", myPid));

			BOOLEAN bRet = MyKillProcess(myPid);
			if (bRet)
			{
				KdPrint(("Kill process Successs!\n"));
			}
			else
			{
				KdPrint(("Failed kill process!\n"));
			}
				 
			PUCHAR outBuffer = pIrp->AssociatedIrp.SystemBuffer;
			memset(outBuffer, 0, outLength);
			memset(outBuffer, 0xBB, outLength / 2);
			info = outLength / 2;
			break;
		}
		default:
		{
			KdPrint(("failed"));
			status = STATUS_UNSUCCESSFUL;
			break;
		}		
	}

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = info;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
	PDEVICE_OBJECT pDevice = NULL;
	KdPrint(("Driver Reg: %ws", reg_path->Buffer));

	// 派遣函数，类似R3的消息循环
	/*
	driver->MajorFunction[IRP_MJ_READ] = MyDriverRead;
	driver->MajorFunction[IRP_MJ_CREATE] = DispatchFunction;
	driver->MajorFunction[IRP_MJ_CLOSE] = DispatchFunction;
	driver->DriverUnload = Unload;
	*/
	driver->DriverUnload = Unload;
	for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		driver->MajorFunction[i] = DispatchFunction;
	}
	driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MyDriverIoctl;

	UNICODE_STRING DeviceName;
	RtlInitUnicodeString(&DeviceName, DEVICE_NAME);
	NTSTATUS status = IoCreateDevice(driver, 0, &DeviceName, FILE_DEVICE_UNKNOWN, 0, TRUE, &pDevice);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Create Device Failed\n"));
		return STATUS_SUCCESS;
	}

	UNICODE_STRING SymLinkName = RTL_CONSTANT_STRING(SIM_LINK_NAME);
	status = IoCreateSymbolicLink(&SymLinkName, &DeviceName);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Create SymbolicLink Failed\n"));
		IoDeleteDevice(pDevice);
		return STATUS_SUCCESS;
	}

	// 读写缓存
	pDevice->Flags |= DO_BUFFERED_IO;

	testPool = ExAllocatePool(PagedPool, 10);
	if (testPool != NULL)
	{
		memset(testPool, 0xbb, 10);
		for (size_t i = 0; i < 10; i++)
		{
			KdPrint(("%02x", *((PCHAR)testPool + i)));
		}
		ExFreePool(testPool);
	}

	noneTestpool = ExAllocatePool(NonPagedPool, 10);
	if (noneTestpool != NULL)
	{
		memset(noneTestpool, 0xcc, 10);
		for (size_t i = 0; i < 10; i++)
		{
			KdPrint(("%02x", *((PCHAR)noneTestpool + i)));
		}
		ExFreePool(noneTestpool);
	}
	noneTestpool = ExAllocatePoolWithTag(NonPagedPool, 10, 'nopg');
	if (noneTestpool)
	{
		ExFreePool(noneTestpool);
	}

	return STATUS_SUCCESS;
}