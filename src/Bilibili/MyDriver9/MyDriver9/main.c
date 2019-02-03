
#include <ntddk.h>


LARGE_INTEGER g_liPreOriginalCounter = {0};
LARGE_INTEGER g_liPreReturnCounter = { 0 };

PVOID orinUpdateTime = NULL;
PVOID orinQuery = NULL;

PVOID orinUpdateTimeCode = NULL;
PVOID orinQueryCode = NULL;

DWORD32 g_dwSpeed_X = 1000;
DWORD32 g_dwSpeedBase = 100;

VOID __declspec(naked) __cdecl JmpOriginal_KeUpdateSystemTime()
{
	__asm
	{
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		mov esi, orinUpdateTime
		add esi, 5
		jmp esi
	}
}

LARGE_INTEGER __declspec(naked) __stdcall JmpOriginal_KeQueryPerformanceCounter(PLARGE_INTEGER PerformanceFrequency)
{
	__asm
	{
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		mov esi, orinQuery
		add esi, 5
		jmp esi
	}
}

VOID __declspec(naked) __cdecl Fake_KeUpdateSystemtime()
{
	__asm
	{
		// 参数为 EAX EDX 中传入
		mul g_dwSpeed_X
		div g_dwSpeedBase
		Jmp JmpOriginal_KeUpdateSystemTime
	}
}

LARGE_INTEGER __stdcall Fake_KeQueryPerformanceCounter(OUT PLARGE_INTEGER PerformanceFrequency)
{
	LARGE_INTEGER liResult;
	LARGE_INTEGER liCurrent;

	liCurrent = JmpOriginal_KeQueryPerformanceCounter(PerformanceFrequency);

	liResult.QuadPart = g_liPreReturnCounter.QuadPart + (liCurrent.QuadPart - g_liPreOriginalCounter.QuadPart) * g_dwSpeed_X / g_dwSpeedBase;;

	g_liPreOriginalCounter.QuadPart = liCurrent.QuadPart;
	g_liPreReturnCounter.QuadPart = liResult.QuadPart;

	return liResult;

}

VOID __declspec(naked) WPOFF()
{
	__asm
	{
		cli
		mov eax, cr0
		and eax, not 0x10000
		mov cr0, eax
		ret
	}
}

VOID __declspec(naked) WPON()
{
	__asm
	{
		mov eax, cr0
		or eax, 0x10000
		mov cr0, eax
		sti
		ret
	}
}

NTSTATUS Unload(PDRIVER_OBJECT driver)
{
	return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
	driver->DriverUnload = Unload;
	
	UNICODE_STRING hookname = RTL_CONSTANT_STRING(L"KeUpdateSystemTime");
	orinUpdateTime = MmGetSystemRoutineAddress(&hookname);
	UNICODE_STRING hookname2 = RTL_CONSTANT_STRING(L"KeQueryPerformanceCounter");
	orinQuery = MmGetSystemRoutineAddress(&hookname2);
	KdPrint(("EpUpdateSystemTime: %p KeQueryPerformanceCounter: %p\n", orinUpdateTime, orinQuery));

	g_liPreOriginalCounter.QuadPart = 0;
	g_liPreReturnCounter.QuadPart = 0;
	g_liPreOriginalCounter = KeQueryPerformanceCounter(NULL);
	g_liPreReturnCounter.QuadPart = g_liPreOriginalCounter.QuadPart;

	UCHAR nJmpCode1[5] = { 0xE9, 0x00, 0x00, 0x00, 0x00 };
	UCHAR nJmpCode2[5] = { 0xE9, 0x00, 0x00, 0x00, 0x00 };
	*(DWORD32 *)(nJmpCode1 + 1) = (DWORD32)Fake_KeUpdateSystemtime - ((DWORD32)orinUpdateTime + 5);
	*(DWORD32 *)(nJmpCode2 + 1) = (DWORD32)Fake_KeQueryPerformanceCounter - ((DWORD32)orinQuery + 5);
		
	orinUpdateTimeCode = ExAllocatePool(NonPagedPool, 20);
	orinQueryCode = ExAllocatePool(NonPagedPool, 20);

	WPOFF();
	KIRQL Irql = KeRaiseIrqlToDpcLevel();
	RtlCopyMemory(orinUpdateTimeCode, orinUpdateTime, 5);
	RtlCopyMemory(JmpOriginal_KeUpdateSystemTime, orinUpdateTime, 5);
	RtlCopyMemory((UCHAR*)orinUpdateTime, nJmpCode1, 5);

	RtlCopyMemory(orinQueryCode, orinQuery, 5);
	RtlCopyMemory(JmpOriginal_KeQueryPerformanceCounter, orinQuery, 5);
	RtlCopyMemory((UCHAR*)orinQuery, nJmpCode2, 5);

	KeLowerIrql(Irql);
	WPON();

	return STATUS_SUCCESS;
}