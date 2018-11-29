
#include "Driver.h"

WCHAR* s_lpDeviceName = L"\\Device\\MyDDKDevice";
WCHAR* s_lpSymbolicName = L"\\??\\HelloDDK";

#pragma INITCODE 
extern "C" NTSTATUS DriverEntry(
		   IN PDRIVER_OBJECT pDriverObject,
		   IN PUNICODE_STRING pRegistryPath)
{
	NTSTATUS status;
    //KdBreakPoint();	
    KdPrint(("Enter DriverEntry\n"));

	pDriverObject->DriverUnload = HelloDDKUnload;
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = HelloDDKDispatchRoutine;
	pDriverObject->MajorFunction[IRP_MJ_READ] = HelloDDKRead;
	pDriverObject->MajorFunction[IRP_MJ_WRITE] = HelloDDKDispatchRoutine;
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
							&(UNICODE_STRING)devName,
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
	if(!NT_SUCCESS(status))
	{
		IoDeleteDevice(pDevObj);
		return status;
	}

	return STATUS_SUCCESS;
}

#pragma PAGECODE
VOID HelloDDKUnload(IN PDRIVER_OBJECT pDriverObject)
{
    //KdBreakPoint();
	PDEVICE_OBJECT pNextObj;
	KdPrint(("Enter DriverUnload\n"));
	pNextObj = pDriverObject->DeviceObject;
	while(pNextObj != NULL)
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
NTSTATUS HelloDDKDispatchRoutine(IN PDEVICE_OBJECT pDevObj,
								 IN PIRP pIrp)
{
	KdPrint(("Enter HelloDDKDispatchRoutine\n"));
	
	static char *irpName[] = {
		"IRP_MJ_CREATE",
		"IRP_MJ_CREATE_NAMED_PIPE",        
		"IRP_MJ_CLOSE",
		"IRP_MJ_READ",
		"IRP_MJ_WRITE",
		"IRP_MJ_QUERY_INFORMATION",
		"IRP_MJ_SET_INFORMATION",
		"IRP_MJ_QUERY_EA",
		"IRP_MJ_SET_EA",
		"IRP_MJ_FLUSH_BUFFERS",
		"IRP_MJ_QUERY_VOLUME_INFORMATION",
		"IRP_MJ_SET_VOLUME_INFORMATION",
		"IRP_MJ_DIRECTORY_CONTROL",
		"IRP_MJ_FILE_SYSTEM_CONTROL",
		"IRP_MJ_DEVICE_CONTROL",
		"IRP_MJ_INTERNAL_DEVICE_CONTROL",
		"IRP_MJ_SHUTDOWN",
		"IRP_MJ_LOCK_CONTROL",
		"IRP_MJ_CLEANUP",
		"IRP_MJ_CREATE_MAILSLOT",
		"IRP_MJ_QUERY_SECURITY",
		"IRP_MJ_SET_SECURITY",
		"IRP_MJ_POWER",
		"IRP_MJ_SYSTEM_CONTROL",
		"IRP_MJ_DEVICE_CHANGE",
		"IRP_MJ_QUERY_QUOTA",
		"IRP_MJ_SET_QUOTA",
		"IRP_MJ_PNP",
		"IRP_MJ_PNP_POWER",
		"IRP_MJ_MAXIMUM_FUNCTION",				
	};

	PIO_STACK_LOCATION pIOStack = IoGetCurrentIrpStackLocation(pIrp);
	UCHAR type = pIOStack->MajorFunction;
	if (type >= arraysize(irpName))
	{
		KdPrint((" - Unknown IRP, major type %X\n", type));
	}
	else 
		KdPrint(("\t%s\n", irpName[type]));

	NTSTATUS status = STATUS_SUCCESS;

	// 完成IRP
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	KdPrint(("Leave HelloDDKDispatchRoutine\n"));
	return status;
}

#pragma PAGECODE
NTSTATUS HelloDDKRead(IN PDEVICE_OBJECT pDevObj,
					  IN PIRP pIrp)
{
	KdPrint(("Enter HelloDDKRead\n"));

	NTSTATUS status = STATUS_SUCCESS;

	PIO_STACK_LOCATION pIOStack = IoGetCurrentIrpStackLocation(pIrp);
	ULONG ulReadLen = pIOStack->Parameters.Read.Length;
	
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = ulReadLen;
	memset(pIrp->AssociatedIrp.SystemBuffer, 0xAA, ulReadLen);
	
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	KdPrint(("Leave HelloDDKRead\n"));
	return status;
}