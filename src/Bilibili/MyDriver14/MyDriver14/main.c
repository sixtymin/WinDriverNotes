
#include <ntddk.h>
#include <windef.h>

typedef LARGE_INTEGER(__cdecl *PKeQueryPerformanceCounter)(PLARGE_INTEGER performace);

PVOID TargetApi = NULL;
PVOID OriginApi = NULL;
BYTE jmp_origin_code[20] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
BYTE HookCode[6] = {0xFF, 0x25, 0xF1, 0xFF, 0xFF, 0xFF};

KIRQL WPOFFx64()
{
	KIRQL irql = KeRaiseIrqlToDpcLevel();
	UINT64 cr0 = __readcr0();
	cr0 &= 0xfffffffffffeffff;
	__writecr0(cr0);
	_disable();
	return irql;
}

void WPONx64(KIRQL irql)
{
	UINT64 cr0 = __readcr0();
	cr0 |= 0x10000;
	_enable();
	__writecr0(cr0);
	KeLowerIrql(irql);
}

NTSTATUS Unload(PDRIVER_OBJECT driver)
{
	UNREFERENCED_PARAMETER(driver);
	KdPrint(("Unload...\n"));

	return STATUS_SUCCESS;
}

LARGE_INTEGER MyQueryPerfor(PLARGE_INTEGER performancefrequency)
{
	PKeQueryPerformanceCounter tempApi = (PKeQueryPerformanceCounter)OriginApi;
	return tempApi(performancefrequency);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
	UNREFERENCED_PARAMETER(driver);
	UNREFERENCED_PARAMETER(reg_path);
	KdPrint(("Enter the driver\n"));
	driver->DriverUnload = Unload;
		
	UNICODE_STRING hookName = RTL_CONSTANT_STRING(L"KeQueryPerformanceCounter");
	TargetApi = MmGetSystemRoutineAddress(&hookName);

	ULONG_PTR MyAddress = (ULONG_PTR)MyQueryPerfor;
	ULONG_PTR OrigAddress = (ULONG_PTR)TargetApi + 0x6;

	RtlCopyMemory(jmp_origin_code, TargetApi, 0x6);
	RtlCopyMemory(jmp_origin_code + 12, &OrigAddress, 8);
	PVOID targetpool = ExAllocatePool(NonPagedPool, 20);
	OriginApi = targetpool;
	RtlZeroMemory(OriginApi, 20);
	RtlCopyMemory(OriginApi, jmp_origin_code, 20);

	KIRQL tempIrql = WPOFFx64();
	RtlCopyMemory((PUCHAR)TargetApi - 0x9, &MyAddress, 8);
	RtlCopyMemory((PUCHAR)TargetApi, HookCode, 6);
	WPONx64(tempIrql);

	return STATUS_SUCCESS;
}