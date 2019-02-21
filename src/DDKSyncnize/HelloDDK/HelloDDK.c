

#include "HelloDDK.h"

WCHAR* s_lpDeviceName = L"\\Device\\MyDDKDevice";
WCHAR* s_lpSymbolicName = L"\\??\\HelloDDK";

#define ID_IOCTL_TEST1 CTL_CODE(\
						FILE_DEVICE_UNKNOWN,\
						0x800,\
						METHOD_BUFFERED,\
						FILE_ANY_ACCESS)

VOID SystemThread(IN PVOID pContext)
{
	UNREFERENCED_PARAMETER(pContext);
	KdPrint(("Enter SystemThread\n"));

	PEPROCESS pEProcess = IoGetCurrentProcess();
	PTSTR ProcessName = (PTSTR)((ULONG)pEProcess + 0x174);
	KdPrint(("This thread run in %s process\n", ProcessName));

	KdPrint(("Leave SystemThread\n"));

	PsTerminateSystemThread(STATUS_SUCCESS);
}

VOID ProcessThread(IN PVOID pContext)
{
	UNREFERENCED_PARAMETER(pContext);
	KdPrint(("Enter ProcessThread\n"));

	PEPROCESS pEProcess = IoGetCurrentProcess();
	PTSTR ProcessName = (PTSTR)((ULONG)pEProcess + 0x174);
	KdPrint(("This thread run in %s process\n", ProcessName));

	KdPrint(("Leave ProcessThread\n"));

	PsTerminateSystemThread(STATUS_SUCCESS);
}

VOID CreateThread_Test()
{
	HANDLE hSystemThread, hProcThread;

	NTSTATUS status = PsCreateSystemThread(&hSystemThread, 0, NULL, NULL, NULL, SystemThread, NULL);
	status = PsCreateSystemThread(&hProcThread, 0, NULL, NtCurrentProcess(), NULL, ProcessThread, NULL);
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
	pDriverObject->MajorFunction[IRP_MJ_WRITE] = HelloDDKIoCtlRoutine;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = HelloDDKDispatchRoutine;

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

#pragma PAGECODE
NTSTATUS HelloDDKIoCtlRoutine(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
	KdPrint(("Enter HelloDDKIoCtlRoutine DevObj: %p\n", pDevObj));
	NTSTATUS status = STATUS_SUCCESS;

	PIO_STACK_LOCATION pIoStack = IoGetCurrentIrpStackLocation(pIrp);
	ULONG cbInBuffer = pIoStack->Parameters.DeviceIoControl.InputBufferLength;
	ULONG cbOutBuffer = pIoStack->Parameters.DeviceIoControl.OutputBufferLength;

	ULONG ctlCode = pIoStack->Parameters.DeviceIoControl.IoControlCode;
	switch (ctlCode)
	{
	case ID_IOCTL_TEST1:
	{
		KdPrint(("Create Thread: \n"));
		CreateThread_Test();		
	}
	default:
		break;
	}

	// 完成IRP
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	KdPrint(("Leave HelloDDKIoCtlRoutine\n"));
	return status;
}