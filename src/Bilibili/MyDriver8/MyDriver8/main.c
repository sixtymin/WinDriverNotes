
#include <ntddk.h>

//extern POBJECT_TYPE* IoDriverObjectType;
extern POBJECT_TYPE *PsThreadType;

BOOLEAN bTerminated = FALSE;
PETHREAD pThreadObj = NULL;
CLIENT_ID ClientId = { 0 };

NTSTATUS
ObReferenceObjectByName(
	IN PUNICODE_STRING ObjectName,
	IN ULONG Attributes,
	IN PACCESS_STATE PassedAccessState,
	IN ACCESS_MASK DesiredAccess,
	IN POBJECT_TYPE ObjectType,
	IN KPROCESSOR_MODE AccessMode,
	IN OUT PVOID ParseContext,
	OUT PVOID *Object
);

//typedef NTSTATUS(*POriginRead)(PDEVICE_OBJECT pdevice, PIRP pirp);

//POriginRead OriginRead = NULL;

NTSTATUS Unload(PDRIVER_OBJECT driver)
{
	UNREFERENCED_PARAMETER(driver);
	KdPrint(("this driver is unloading...\n"));
	bTerminated = TRUE;
	KeWaitForSingleObject(pThreadObj, Executive, KernelMode, FALSE, NULL);
    ObDereferenceObject(pThreadObj);
	/*if (NULL != OriginRead)
	{
		driver->MajorFunction[IRP_MJ_READ] = OriginRead;
	}*/	

	return STATUS_SUCCESS;
}

/*
NTSTATUS myreadpatch(PDEVICE_OBJECT device, PIRP irp)
{
	KdPrint(("-----read-----"));

	return OriginRead(device, irp);
}*/

VOID MyThread(PVOID lpParam)
{
	UNREFERENCED_PARAMETER(lpParam);
	LARGE_INTEGER interval;
	interval.QuadPart = -20000000;
	while (1)
	{
		KdPrint(("=====MyThrad=======\n"));
		if (bTerminated)
		{
			break;
		}
		KeDelayExecutionThread(KernelMode, FALSE, &interval);
	}
	PsTerminateSystemThread(STATUS_SUCCESS); // 内核线程不会自己结束
}


NTSTATUS CreateMyThread(PVOID targep)
{
	UNREFERENCED_PARAMETER(targep);
	OBJECT_ATTRIBUTES ObjAddr = {0};
	HANDLE ThreadHandle = 0;
	NTSTATUS ntStatus = STATUS_SUCCESS;
	InitializeObjectAttributes(&ObjAddr, NULL, OBJ_KERNEL_HANDLE, 0, NULL);
	ntStatus = PsCreateSystemThread(&ThreadHandle, THREAD_ALL_ACCESS, &ObjAddr, NULL, NULL, MyThread, NULL);
	if (NT_SUCCESS(ntStatus))
	{
		KdPrint(("Thread Createed\n"));
		ntStatus = ObReferenceObjectByHandle(ThreadHandle, THREAD_ALL_ACCESS, *PsThreadType, KernelMode, &pThreadObj, NULL);
		ZwClose(ThreadHandle);
		if (!NT_SUCCESS(ntStatus))
		{
			bTerminated = TRUE;
		}		
	}

	return ntStatus;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
	KdPrint(("Driver Reg: %ws", reg_path->Buffer));

	driver->DriverUnload = Unload;
	/*
	PDRIVER_OBJECT kbddriver = NULL;
	UNICODE_STRING kbdname = RTL_CONSTANT_STRING(L"\\Driver\\Kbdclass");
	NTSTATUS status = ObReferenceObjectByName(&kbdname, OBJ_CASE_INSENSITIVE, NULL, 0, *IoDriverObjectType, KernelMode, NULL, &kbddriver);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Open driver failed\n"));
		return STATUS_SUCCESS;
	}
	else
	{
		ObDereferenceObject(kbddriver);
	}

	OriginRead = (POriginRead)kbddriver->MajorFunction[IRP_MJ_READ];
	kbddriver->MajorFunction[IRP_MJ_READ] = myreadpatch; */

	CreateMyThread(NULL);

	return STATUS_SUCCESS;
}