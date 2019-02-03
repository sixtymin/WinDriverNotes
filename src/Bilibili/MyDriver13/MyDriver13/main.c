
#include <ntddk.h>
#include <intrin.h>

typedef struct _IMAGE_FILE_HEADER // Size=20
{
	USHORT Machine;
	USHORT NumberOfSections;
	ULONG TimeDateStamp;
	ULONG PointerToSymbolTable;
	ULONG NumberOfSymbols;
	USHORT SizeOfOptionalHeader;
	USHORT Characteristics;
} IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;

typedef struct _IMAGE_SECTION_HEADER
{
	UCHAR  Name[8];
	union
	{
		ULONG PhysicalAddress;
		ULONG VirtualSize;
	} Misc;
	ULONG VirtualAddress;
	ULONG SizeOfRawData;
	ULONG PointerToRawData;
	ULONG PointerToRelocations;
	ULONG PointerToLinenumbers;
	USHORT  NumberOfRelocations;
	USHORT  NumberOfLinenumbers;
	ULONG Characteristics;
} IMAGE_SECTION_HEADER, *PIMAGE_SECTION_HEADER;

typedef struct _IMAGE_DATA_DIRECTORY
{
	ULONG VirtualAddress;
	ULONG Size;
} IMAGE_DATA_DIRECTORY, *PIMAGE_DATA_DIRECTORY;

typedef struct _RTL_PROCESS_MODULE_INFORMATION
{
	HANDLE Section;         // Not filled in
	PVOID MappedBase;
	PVOID ImageBase;
	ULONG ImageSize;
	ULONG Flags;
	USHORT LoadOrderIndex;
	USHORT InitOrderIndex;
	USHORT LoadCount;
	USHORT OffsetToFileName;
	UCHAR  FullPathName[256];
} RTL_PROCESS_MODULE_INFORMATION, *PRTL_PROCESS_MODULE_INFORMATION;

typedef struct _RTL_PROCESS_MODULES
{
	ULONG NumberOfModules;
	RTL_PROCESS_MODULE_INFORMATION Modules[1];
} RTL_PROCESS_MODULES, *PRTL_PROCESS_MODULES;

typedef enum _SYSTEM_INFORMATION_CLASS
{
	SystemModuleInformation = 0xb,
} SYSTEM_INFORMATION_CLASS;

#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES    16

// Optional header format.
typedef struct _IMAGE_OPTIONAL_HEADER {
	USHORT  Magic;
	UCHAR   MajorLinkerVersion;
	UCHAR   MinorLinkerVersion;
	ULONG   SizeOfCode;
	ULONG   SizeOfInitializedData;
	ULONG   SizeOfUninitializedData;
	ULONG   AddressOfEntryPoint;
	ULONG   BaseOfCode;
	ULONG   BaseOfData;
	ULONG   ImageBase;
	ULONG   SectionAlignment;
	ULONG   FileAlignment;
	USHORT  MajorOperatingSystemVersion;
	USHORT  MinorOperatingSystemVersion;
	USHORT  MajorImageVersion;
	USHORT  MinorImageVersion;
	USHORT  MajorSubsystemVersion;
	USHORT  MinorSubsystemVersion;
	ULONG   Win32VersionValue;
	ULONG   SizeOfImage;
	ULONG   SizeOfHeaders;
	ULONG   CheckSum;
	USHORT  Subsystem;
	USHORT  DllCharacteristics;
	ULONG   SizeOfStackReserve;
	ULONG   SizeOfStackCommit;
	ULONG   SizeOfHeapReserve;
	ULONG   SizeOfHeapCommit;
	ULONG   LoaderFlags;
	ULONG   NumberOfRvaAndSizes;
	IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} IMAGE_OPTIONAL_HEADER32, *PIMAGE_OPTIONAL_HEADER32;

typedef struct _IMAGE_NT_HEADERS {
	ULONG Signature;
	IMAGE_FILE_HEADER FileHeader;
	IMAGE_OPTIONAL_HEADER32 OptionalHeader;
} IMAGE_NT_HEADERS32, *PIMAGE_NT_HEADERS32;

#define HB_POOL_TAG                '0mVZ'

typedef NTSTATUS(__stdcall *PMiProcessLoaderEntry)(PVOID pDriverSection, int bLoad);

PVOID MiProcessLoaderEntry1 = NULL;	
BOOLEAN TempFlags = FALSE;

PVOID g_KernelBase = NULL;
ULONG  g_KernelSize = 0;

NTSTATUS __stdcall ZwQuerySystemInformation(
	_In_      SYSTEM_INFORMATION_CLASS SystemInformationClass,
	_Inout_   PVOID                    SystemInformation,
	_In_      ULONG                    SystemInformationLength,
	_Out_opt_ PULONG                   ReturnLength
);

PIMAGE_NT_HEADERS __stdcall RtlImageNtHeader(IN PVOID ModuleAddress);

PVOID UtilKernelBase(OUT PULONG pSize)
{
	NTSTATUS status = STATUS_SUCCESS;
	ULONG bytes = 0;
	PRTL_PROCESS_MODULES pMods = NULL;
	PVOID checkPtr = NULL;
	UNICODE_STRING routineName;

	// Already found
	if (g_KernelBase != NULL)
	{
		if (pSize)
			*pSize = g_KernelSize;
		return g_KernelBase;
	}

	RtlInitUnicodeString(&routineName, L"NtOpenFile");

	checkPtr = MmGetSystemRoutineAddress(&routineName);
	if (checkPtr == NULL)
		return NULL;

	// Protect from UserMode AV
	__try
	{
		status = ZwQuerySystemInformation(SystemModuleInformation, 0, bytes, &bytes);
		if (bytes == 0)
		{
			//DPRINT("BlackBone: %s: Invalid SystemModuleInformation size\n", CPU_IDX, __FUNCTION__);
			return NULL;
		}

		pMods = (PRTL_PROCESS_MODULES)ExAllocatePoolWithTag(NonPagedPoolNx, bytes, HB_POOL_TAG);
		RtlZeroMemory(pMods, bytes);

		status = ZwQuerySystemInformation(SystemModuleInformation, pMods, bytes, &bytes);

		if (NT_SUCCESS(status))
		{
			PRTL_PROCESS_MODULE_INFORMATION pMod = pMods->Modules;

			for (ULONG i = 0; i < pMods->NumberOfModules; i++)
			{
				// System routine is inside module
				if (checkPtr >= pMod[i].ImageBase &&
					checkPtr < (PVOID)((PUCHAR)pMod[i].ImageBase + pMod[i].ImageSize))
				{
					g_KernelBase = pMod[i].ImageBase;
					g_KernelSize = pMod[i].ImageSize;
					if (pSize)
						*pSize = g_KernelSize;
					break;
				}
			}
		}

	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		//DPRINT("BlackBone: %s: Exception\n", CPU_IDX, __FUNCTION__);
	}

	if (pMods)
		ExFreePoolWithTag(pMods, HB_POOL_TAG);

	return g_KernelBase;
}

NTSTATUS UtilSearchPattern(IN PCUCHAR pattern, IN UCHAR wildcard, IN ULONG_PTR len, IN const VOID* base, IN ULONG_PTR size, OUT PVOID* ppFound)
{
	NT_ASSERT(ppFound != NULL && pattern != NULL && base != NULL);
	if (ppFound == NULL || pattern == NULL || base == NULL)
		return STATUS_INVALID_PARAMETER;

	__try
	{
		for (ULONG_PTR i = 0; i < size - len; i++)
		{
			BOOLEAN found = TRUE;
			for (ULONG_PTR j = 0; j < len; j++)
			{
				if (pattern[j] != wildcard && pattern[j] != ((PCUCHAR)base)[i + j])
				{
					found = FALSE;
					break;
				}
			}

			if (found != FALSE)
			{
				*ppFound = (PUCHAR)base + i;
				return STATUS_SUCCESS;
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return STATUS_UNHANDLED_EXCEPTION;
	}

	return STATUS_NOT_FOUND;
}


NTSTATUS UtilScanSection(IN PCCHAR section, IN PCUCHAR pattern, IN UCHAR wildcard, IN ULONG_PTR len, OUT PVOID* ppFound)
{
	NT_ASSERT(ppFound != NULL);
	if (ppFound == NULL)
		return STATUS_INVALID_PARAMETER;

	PVOID base = UtilKernelBase(NULL);
	if (!base)
		return STATUS_NOT_FOUND;

	PIMAGE_NT_HEADERS32 pHdr = (PIMAGE_NT_HEADERS32)RtlImageNtHeader(base);
	if (!pHdr)
		return STATUS_INVALID_IMAGE_FORMAT;

	PIMAGE_SECTION_HEADER pFirstSection = (PIMAGE_SECTION_HEADER)((PUCHAR)pHdr + sizeof(IMAGE_NT_HEADERS32));
	for (PIMAGE_SECTION_HEADER pSection = pFirstSection; pSection < pFirstSection + pHdr->FileHeader.NumberOfSections; pSection++)
	{
		ANSI_STRING s1, s2;
		RtlInitAnsiString(&s1, section);
		RtlInitAnsiString(&s2, (PCCHAR)pSection->Name);
		if (RtlCompareString(&s1, &s2, FALSE) == 0)
			return UtilSearchPattern(pattern, wildcard, len, (PUCHAR)base + pSection->VirtualAddress, pSection->Misc.VirtualSize, ppFound);
	}

	return STATUS_NOT_FOUND;
}

NTSTATUS Unload(PDRIVER_OBJECT driver)
{
	KdPrint(("Unload nothing to do\n"));

	return STATUS_SUCCESS;
}

VOID HideDriver(PVOID lpParam)
{
	PDRIVER_OBJECT DriverObject = NULL;
	LARGE_INTEGER SleepTime;
	SleepTime.QuadPart = -20 * 1000 * 1000;  // 2s
	while (1)
	{
		if (TempFlags)
		{
			break;
		}
		KeDelayExecutionThread(KernelMode, 0, &SleepTime);
	}
	KeDelayExecutionThread(KernelMode, 0, &SleepTime);

	CHAR pattern[] = "\x8b\xff\x55\x8b\xec\x53\x56\x57\x6a\x01\xbb\x40\xe7\x55\x80\x53\xe8\x97\x59\x02\x00\xbf\x80\x4c\x55\x80\x8b\xcf";
	int sz = sizeof(pattern) - 1;
	KdPrint(("sz: %d\n", sz));
	UtilScanSection(".text", (PCUCHAR)pattern, 0xCC, sz, (PVOID*)&MiProcessLoaderEntry1);
	KdPrint(("MiProcessLoaderEntry addr: %p", MiProcessLoaderEntry1));
	if (MiProcessLoaderEntry1 == NULL)
	{
		PsTerminateSystemThread(STATUS_SUCCESS);
		return;
	}

	DriverObject = (PDRIVER_OBJECT)lpParam;
	if (NULL != DriverObject)
	{
		PMiProcessLoaderEntry tempcall = (PMiProcessLoaderEntry)MiProcessLoaderEntry1;
		tempcall(DriverObject->DriverSection, 0);
		DriverObject->DriverSection = NULL;
		DriverObject->DriverStart = NULL;
		DriverObject->DriverSize = 0;
		DriverObject->DriverUnload = NULL;
	}

	PsTerminateSystemThread(STATUS_SUCCESS);
	return ;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
	KdPrint(("Enter the driver\n"));
	driver->DriverUnload = Unload;
	
	HANDLE hThread;
	KdPrint(("Run To Here!\n"));
	NTSTATUS status = PsCreateSystemThread(&hThread, 0, NULL, NULL, NULL, HideDriver, (PVOID)driver);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Create Thread Error!\n"));
	}
	ZwClose(hThread);
	TempFlags = TRUE;

	return STATUS_SUCCESS;
}