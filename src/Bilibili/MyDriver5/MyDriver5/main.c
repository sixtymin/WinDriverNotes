
#include <wdm.h>
#include <ntddkbd.h>

extern POBJECT_TYPE* IoDriverObjectType;

#define KTD_NAME L"\\Device\\Kbdclass"

//键盘计数，用于标记是否irp完成
ULONG KS_KeyCount;

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

typedef struct _Dev_exten
{
	ULONG size;

	PDEVICE_OBJECT filterdevice;
	PDEVICE_OBJECT targetdevice;
	PDEVICE_OBJECT lowdevice;

	KSPIN_LOCK IoRequestsSpinLock;

	KEVENT IoInProgressEvent;

}DEV_EXTEN, *PDEV_EXTEN;

NTSTATUS entrydevices(PDEV_EXTEN devext, PDEVICE_OBJECT filterdevice, PDEVICE_OBJECT targetdevice, PDEVICE_OBJECT lowdevice)
{
	memset(devext, 0, sizeof(DEV_EXTEN));
	devext->filterdevice = filterdevice;
	devext->targetdevice = targetdevice;
	devext->lowdevice = lowdevice;
	devext->size = sizeof(DEV_EXTEN);
	KeInitializeSpinLock(&devext->IoRequestsSpinLock);
	KeInitializeEvent(&devext->IoInProgressEvent, NotificationEvent, FALSE);
	return STATUS_SUCCESS;
}

NTSTATUS attachDevice(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
	NTSTATUS status;

	UNREFERENCED_PARAMETER(reg_path);

	UNICODE_STRING kbdname = RTL_CONSTANT_STRING(L"\\Driver\\Kbdclass");
	PDEV_EXTEN drvext;
	PDEVICE_OBJECT filterdevice;
	PDEVICE_OBJECT targetdevice;
	PDEVICE_OBJECT lowdevice;
	PDRIVER_OBJECT kbddriver;

	status = ObReferenceObjectByName(&kbdname, OBJ_CASE_INSENSITIVE, NULL, 0, *IoDriverObjectType, KernelMode, NULL, &kbddriver);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Open driver failed\n"));
		return status;
	}
	else
	{
		ObDereferenceObject(kbddriver);
	}

	targetdevice = kbddriver->DeviceObject;
	while (targetdevice)
	{
		status = IoCreateDevice(driver, sizeof(DEV_EXTEN), NULL, targetdevice->DeviceType, targetdevice->Characteristics, FALSE, &filterdevice);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("create device failed\n"));
			filterdevice = NULL;
			targetdevice = NULL;
			return status;
		}
		lowdevice = IoAttachDeviceToDeviceStack(filterdevice, targetdevice);
		if (!lowdevice)
		{
			KdPrint(("Attach Failed\n"));
			filterdevice = NULL;
		}

		drvext = (PDEV_EXTEN)filterdevice->DeviceExtension;
		entrydevices(drvext, filterdevice, targetdevice, lowdevice);
		filterdevice->DeviceType = lowdevice->DeviceType;
		filterdevice->Characteristics = lowdevice->Characteristics;
		filterdevice->StackSize = lowdevice->StackSize + 1;
		filterdevice->Flags |= lowdevice->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE);
		targetdevice = targetdevice->NextDevice;
	}
	KdPrint(("create and attach finished.\n"));
	return STATUS_SUCCESS;
}

NTSTATUS mydispatch(PDEVICE_OBJECT pdevice, PIRP pIrp)
{	
	NTSTATUS status;
	KdPrint(("go to lowdevice\n"));
	PDEV_EXTEN devext = (PDEV_EXTEN)pdevice->DeviceExtension;
	PDEVICE_OBJECT lowdevice = devext->lowdevice;
	IoSkipCurrentIrpStackLocation(pIrp);
	status = IoCallDriver(lowdevice, pIrp);
	return status;
}

NTSTATUS mypowerpatch(PDEVICE_OBJECT pdevice, PIRP pIrp)
{
	NTSTATUS status;
	KdPrint(("go to power\n"));
	PDEV_EXTEN devext = (PDEV_EXTEN)pdevice->DeviceExtension;
	PDEVICE_OBJECT lowdevice = devext->lowdevice;
	status = IoCallDriver(lowdevice, pIrp);

	return status;
}

NTSTATUS mypnppatch(PDEVICE_OBJECT pdevice, PIRP pIrp)
{
	NTSTATUS status;
	KdPrint(("go to pnp\n"));
	PDEV_EXTEN devext = (PDEV_EXTEN)pdevice->DeviceExtension;
	PDEVICE_OBJECT lowdevice = devext->lowdevice;
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrp);
	switch (stack->MinorFunction)
	{
	case IRP_MN_REMOVE_DEVICE:
	{
		IoSkipCurrentIrpStackLocation(pIrp);
		IoCallDriver(lowdevice, pIrp);
		IoDetachDevice(lowdevice);
		IoDeleteDevice(pdevice);
		status = STATUS_SUCCESS;
		break;
	}		
	default:
		IoSkipCurrentIrpStackLocation(pIrp);
		status = IoCallDriver(lowdevice, pIrp);
		break;
	}
	return status;
}

NTSTATUS myreadcomplete(PDEVICE_OBJECT pdevice, PIRP pIrp, PVOID Context)
{
	UNREFERENCED_PARAMETER(pdevice);
	UNREFERENCED_PARAMETER(Context);

	PIO_STACK_LOCATION stack;
	ULONG keys;
	PKEYBOARD_INPUT_DATA mydata;
	stack = IoGetCurrentIrpStackLocation(pIrp);
	if (NT_SUCCESS(pIrp->IoStatus.Status))
	{
		mydata = pIrp->AssociatedIrp.SystemBuffer;
		keys = pIrp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA);
		for (ULONG i = 0; i < keys; i++)
		{
			KdPrint(("numkeys: %d\n", keys));
			KdPrint(("samcode: %d\n", mydata->MakeCode));
			KdPrint(("%s\n", mydata->Flags ? "Up" : "Down"));
			if (mydata->MakeCode == 0x1F)   // 修改内容
			{
				mydata->MakeCode = 0x20;
			}
			mydata++;
		}
	}

	KS_KeyCount--;
	if (pIrp->PendingReturned)
	{
		IoMarkIrpPending(pIrp);
	}
	return pIrp->IoStatus.Status;
}

NTSTATUS ReadDispatch(PDEVICE_OBJECT pDevice, PIRP pIrp)
{
	NTSTATUS status = STATUS_SUCCESS;

	PDEV_EXTEN devExt;
	PDEVICE_OBJECT lowDevice;
	PIO_STACK_LOCATION stack;

	if (pIrp->CurrentLocation == 1)
	{
		KdPrint(("irp send error..\n"));
		status = STATUS_INVALID_DEVICE_REQUEST;
		pIrp->IoStatus.Status = status;
		pIrp->IoStatus.Information = 0;
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		return status;
	}

	devExt = pDevice->DeviceExtension;
	lowDevice = devExt->lowdevice;
	stack = IoGetCurrentIrpStackLocation(pIrp);
	
	KS_KeyCount++;
	IoCopyCurrentIrpStackLocationToNext(pIrp);
	IoSetCompletionRoutine(pIrp, myreadcomplete, pDevice, TRUE, TRUE, TRUE);
	status = IoCallDriver(lowDevice, pIrp);
	return status;
}

NTSTATUS dettach(PDEVICE_OBJECT pdevice)
{
	PDEV_EXTEN devext = (PDEV_EXTEN)pdevice->DeviceExtension;
	IoDetachDevice(devext->targetdevice);
	devext->targetdevice = NULL;
	IoDeleteDevice(pdevice);
	devext->filterdevice = NULL;
	return STATUS_SUCCESS;
}

NTSTATUS DriverUnload(PDRIVER_OBJECT driver)
{
	PDEVICE_OBJECT pdevice;
	pdevice = driver->DeviceObject;

	while (pdevice)
	{
		dettach(pdevice);
		pdevice = pdevice->NextDevice;
	}

	LARGE_INTEGER KS_Delay;
	KS_Delay = RtlConvertLongToLargeInteger(-10 * 3000000);
	while (KS_KeyCount)
	{
		KeDelayExecutionThread(KernelMode, FALSE, &KS_Delay);
	}
		
	KdPrint(("driver is unloading...\n"));
	return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
	ULONG i;
	NTSTATUS status;

	for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		driver->MajorFunction[i] = mydispatch;
	}

	driver->MajorFunction[IRP_MJ_READ] = ReadDispatch;
	driver->MajorFunction[IRP_MJ_POWER] = mypowerpatch;
	driver->MajorFunction[IRP_MJ_PNP] = mypnppatch;

	KdPrint(("driver entry\n"));
	driver->DriverUnload = DriverUnload;
	status = attachDevice(driver, reg_path);

	return STATUS_SUCCESS;
}