
#include "Driver.h"

WCHAR* s_lpDeviceName = L"\\Device\\MyDDKDevice";
WCHAR* s_lpSymbolicName = L"\\??\\HelloDDK";

VOID LinkListTest();
VOID RtlTest();

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
	pDriverObject->MajorFunction[IRP_MJ_READ] = HelloDDKDispatchRoutine;
	pDriverObject->MajorFunction[IRP_MJ_WRITE] = HelloDDKDispatchRoutine;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = HelloDDKDispatchRoutine;

	status = CreateDevice(pDriverObject);

	LinkListTest();

	RtlTest();

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
	NTSTATUS status = STATUS_SUCCESS;

	// 完成IRP
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	KdPrint(("Leave HelloDDKDispatchRoutine\n"));
	return status;
}

typedef struct _MYDATA_STRUCT
{
	ULONG number;
	int x;
	int y;
	LIST_ENTRY ListEntry;
}MYDATASTRUCT, *PMYDATASTRUCT;

#pragma PAGECODE
VOID LinkListTest()
{
	LIST_ENTRY linkListHead;

	InitializeListHead(&linkListHead);
	
	PMYDATASTRUCT pData;
	ULONG i = 0;
	KdPrint(("Begin insert to link list\n"));
	for (i = 0; i < 10; i++)
	{
		pData = (PMYDATASTRUCT)ExAllocatePool(PagedPool, sizeof(MYDATASTRUCT));
		pData->number = i;

		InsertHeadList(&linkListHead, &pData->ListEntry);
	}

	KdPrint(("Begin remove from link list\n"));
	while(!IsListEmpty(&linkListHead))
	{
		PLIST_ENTRY pEntry = RemoveTailList(&linkListHead);
		pData = CONTAINING_RECORD(pEntry, 
								  MYDATASTRUCT,
								  ListEntry);

		KdPrint(("ListNumValue: %d\n", pData->number));
		ExFreePool(pData);
	}

	return ;
}

#define BUFFER_SIZE 1024

#pragma PAGECODE
VOID RtlTest()
{
	PUCHAR pBuffer = (PUCHAR)ExAllocatePool(PagedPool, BUFFER_SIZE);

	RtlZeroMemory(pBuffer, BUFFER_SIZE);

	PUCHAR pBuffer2 = (PUCHAR)ExAllocatePool(PagedPool, BUFFER_SIZE);
	RtlFillMemory(pBuffer2, BUFFER_SIZE, 0xAA);

	RtlCopyMemory(pBuffer, pBuffer2, BUFFER_SIZE);
	ULONG ulRet = RtlCompareMemory(pBuffer, pBuffer2, BUFFER_SIZE);
	if (ulRet == BUFFER_SIZE)
	{
		KdPrint(("The Tow Blocks are same!\n"));
	}

	PVOID badPointer = NULL;
	KdPrint(("Enter ProbeTest\n"));

	__try
	{
		KdPrint(("Entry __try block\n"));

		ProbeForWrite(badPointer, 100, 4);

		KdPrint(("Leave __try block\n"));
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		KdPrint(("Catch the exception\n"));
	}

	return ;
}
