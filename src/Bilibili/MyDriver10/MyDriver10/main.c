
#include <ntddk.h>

NTSTATUS PsLookupProcessByProcessId(
	HANDLE    ProcessId,
	PEPROCESS *Process
);

VOID MyProcessCallBack(HANDLE ParentID, HANDLE ProcessId, BOOLEAN bCreate)
{
	if (bCreate)
	{
		PEPROCESS Process = NULL;
		NTSTATUS status = PsLookupProcessByProcessId(ProcessId, &Process);
		int i;
		if (NT_SUCCESS(status))
		{
			for (i = 0; i < 3 * PAGE_SIZE; i++)
			{
				if (!strncmp("notepad.exe", (PCHAR)Process + i, strlen("notepad.exe")))
				{
					if (i < 3 * PAGE_SIZE)
					{
						KdPrint(("Process Name: %s", (PCHAR)((ULONG)Process + i)));
						if (*((PUCHAR)Process + 0xBC) != 0)
						{
							KdPrint(("The notepad.exe is be debugging\n"));
						}
						break;
					}
				}
			}
		}

	}
}

VOID MyLoadImageCallback(PUNICODE_STRING imagename, HANDLE ProcessId, PIMAGE_INFO imageinfo)
{
	KdPrint(("Proc: %d %ws--- start address is %p", ProcessId, imagename->Buffer, imageinfo->ImageBase));
}

NTSTATUS Unload(PDRIVER_OBJECT driver)
{
	NTSTATUS status = PsSetCreateProcessNotifyRoutine(MyProcessCallBack, TRUE);
	status = PsRemoveLoadImageNotifyRoutine(MyLoadImageCallback);
	return STATUS_SUCCESS;
}


NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
	KdPrint(("Enter the driver\n"));
	driver->DriverUnload = Unload;
	
	NTSTATUS status = PsSetCreateProcessNotifyRoutine(MyProcessCallBack, FALSE);
	status = PsSetLoadImageNotifyRoutine(MyLoadImageCallback);

	return STATUS_SUCCESS;
}