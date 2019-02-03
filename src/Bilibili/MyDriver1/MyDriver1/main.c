
#include <ntddk.h>

NTSTATUS DriverUnload(PDRIVER_OBJECT lpDrvObj)
{	
	KdPrint(("Driver Unload 0x%08X", lpDrvObj));
	return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT lpDrvObj, PUNICODE_STRING lpRegPath)
{
	//UNREFERENCED_PARAMETER(lpDrvObj);
	//UNREFERENCED_PARAMETER(lpRegPath);

	lpDrvObj->DriverUnload = DriverUnload;

	KdPrint(("RegPath: %ws", lpRegPath->Buffer));

	return STATUS_SUCCESS;
}