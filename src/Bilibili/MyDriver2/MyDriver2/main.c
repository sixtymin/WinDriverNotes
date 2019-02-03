
#include <ntddk.h>

#define DEVICE_NAME   L"\\Device\\MyReadDevice"
#define SIM_LINK_NAME L"\\??\\MyRead"


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

NTSTATUS MyDriverRead(PDEVICE_OBJECT pDevice, PIRP pIrp)
{
	UNREFERENCED_PARAMETER(pDevice);
	NTSTATUS status;

	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrp);
	ULONG ulReadLength = stack->Parameters.Read.Length;

	memset(pIrp->AssociatedIrp.SystemBuffer, 0xAA, ulReadLength);

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = ulReadLength;	
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	KdPrint(("OVER\n"));
	status = STATUS_SUCCESS;

	return status;
}

NTSTATUS DispatchFunction(PDEVICE_OBJECT pDevice, PIRP pIrp)
{
	UNREFERENCED_PARAMETER(pDevice);

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
	PDEVICE_OBJECT pDevice = NULL;
	KdPrint(("Driver Reg: %ws", reg_path->Buffer));

	// 派遣函数，类似R3的消息循环
	driver->MajorFunction[IRP_MJ_READ] = MyDriverRead;
	driver->MajorFunction[IRP_MJ_CREATE] = DispatchFunction;
	driver->MajorFunction[IRP_MJ_CLOSE] = DispatchFunction;
	driver->DriverUnload = Unload;

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

	return STATUS_SUCCESS;
}