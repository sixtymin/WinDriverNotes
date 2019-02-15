
#派遣函数#

学习到目前为止，对驱动的理解就是它其实只是系统的`I/O`的扩展，主要功能是负责处理`I/O`请求。从前面驱动的简单结构可知，驱动中的`I/O`请求大部分是派遣函数在处理，用户模式下对驱动的`I/O`请求全部由操作系统转化为一个被称为`IRP`的数据结构，不同的IRP被`派遣`到不同的派遣函数中。

每个IRP中包含了两个数据，`MajorFunction`和`MinorFunction`，它们分别记录了IRP的主类型和子类型。前面学习的两类驱动中都在`DriverEntry`中注册了IRP的派遣函数。

> 其实`MajorFunction`就有很多种类，在`DriverEntry`中只赋值了部分函数，其他的会被默认设置为`_IopInvalidDeviceRequest`函数指针，该函数有人逆向并写出了C语言函数，如下。

```
NTSTATUS IopInvalidDeviceRequest(
	IN PDEVICE_OBJECT pDeviceObject,
	IN PIRP Irp)
{
	if (Irp->Tail.Overlay.CurrentStackLocation->MajorFunction == IRP_MJ_POWER) {
		PoStartNextPowerIrp(Irp);
	}

	Irp->IoStatus.Status = 0C0000010h
	IofCompleteRequest(Irp, IO_NO_INCREMENT);
	return Irp->IoStatus.Status;
}
```

**派遣函数简单处理**

派遣函数要处理IRP，一般情况下都是要完成某项功能。如果不想做任何功能，可以用最简单的方法来处理IRP：在派遣函数中将IRP的状态设置为成功，然后结束IRP的请求，并让派遣函数返回成功。如下代码块给出了最简单处理IRP的方法。

```
#pragma PAGECODE
NTSTATUS HelloDDKDispatchRoutine(IN PDEVICE_OBJECT pDevObj,
								 IN PIRP pIrp)
{
	KdPrint(("Enter HelloDDKDispatchRoutine\n"));
	NTSTATUS status = STATUS_SUCCESS;

	// 完成IRP
	pIrp->IoStatus.Status = status;             // IRP状态设置为成功
	pIrp->IoStatus.Information = 0;             // 返回数据大小设置为0
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);   // 结束IRP请求
	KdPrint(("Leave HelloDDKDispatchRoutine\n"));
	return status;                              // 派遣函数也返回成功
}
```

###`IRP_MJ_READ`派遣函数###

在NtDriver中给所有的派遣函数设置了一个默认的派遣函数，派遣函数就是简单设置IRP状态，将IRP设置为完成，并且让派遣函数返回成功。

如下给出一个完善`IRP_MJ_READ`函数的例子`HelloDDKRead`，该函数中除了将IRP完成之外，给返回信息中设置读取数据长度为函数中处理数据的长度，将读取数据的缓存设置为全`0xAA`，然后返回成功。

```
#pragma PAGECODE
NTSTATUS HelloDDKRead(IN PDEVICE_OBJECT pDevObj,
					  IN PIRP pIrp)
{
	KdPrint(("Enter HelloDDKRead\n"));

	NTSTATUS status = STATUS_SUCCESS;

	PIO_STACK_LOCATION pIOStack = IoGetCurrentIrpStackLocation(pIrp);
	ULONG ulReadLen = pIOStack->Parameters.Read.Length;

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = ulReadLen;
	memset(pIrp->AssociatedIrp.SystemBuffer, 0xAA, ulReadLen);

	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	KdPrint(("Leave HelloDDKRead\n"));
	return status;
}

#pragma PAGECODE
NTSTATUS HelloDDKDispatchRoutine(IN PDEVICE_OBJECT pDevObj,
								 IN PIRP pIrp)
{
	KdPrint(("Enter HelloDDKDispatchRoutine\n"));

	static char *irpName[] = {
		"IRP_MJ_CREATE",
		"IRP_MJ_CREATE_NAMED_PIPE",
		"IRP_MJ_CLOSE",
		"IRP_MJ_READ",
		"IRP_MJ_WRITE",
		"IRP_MJ_QUERY_INFORMATION",
		"IRP_MJ_SET_INFORMATION",
		"IRP_MJ_QUERY_EA",
		"IRP_MJ_SET_EA",
		"IRP_MJ_FLUSH_BUFFERS",
		"IRP_MJ_QUERY_VOLUME_INFORMATION",
		"IRP_MJ_SET_VOLUME_INFORMATION",
		"IRP_MJ_DIRECTORY_CONTROL",
		"IRP_MJ_FILE_SYSTEM_CONTROL",
		"IRP_MJ_DEVICE_CONTROL",
		"IRP_MJ_INTERNAL_DEVICE_CONTROL",
		"IRP_MJ_SHUTDOWN",
		"IRP_MJ_LOCK_CONTROL",
		"IRP_MJ_CLEANUP",
		"IRP_MJ_CREATE_MAILSLOT",
		"IRP_MJ_QUERY_SECURITY",
		"IRP_MJ_SET_SECURITY",
		"IRP_MJ_POWER",
		"IRP_MJ_SYSTEM_CONTROL",
		"IRP_MJ_DEVICE_CHANGE",
		"IRP_MJ_QUERY_QUOTA",
		"IRP_MJ_SET_QUOTA",
		"IRP_MJ_PNP",
		"IRP_MJ_PNP_POWER",
		"IRP_MJ_MAXIMUM_FUNCTION",
	};

	PIO_STACK_LOCATION pIOStack = IoGetCurrentIrpStackLocation(pIrp);
	UCHAR type = pIOStack->MajorFunction;
	if (type >= arraysize(irpName))
	{
		KdPrint((" - Unknown IRP, major type %X\n", type));
	}
	else
		KdPrint(("\t%s\n", irpName[type]));

	NTSTATUS status = STATUS_SUCCESS;

	// 完成IRP
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	KdPrint(("Leave HelloDDKDispatchRoutine\n"));
	return status;
}
```

下面编写一个打开驱动，并且从驱动读取数据的例子，如下代码块。在驱动中将默认的派遣函数修改一下，将调用到派遣函数的IRP的主类型号打印一下。这样可以方便看到被处理的IRP类型。

```
#include "stdafx.h"
#include <Windows.h>
#include <stdio.h>

int _tmain(int argc, _TCHAR* argv[])
{
	HANDLE hDevice = CreateFileW(L"\\\\.\\HelloDDK",
								 GENERIC_READ,
								 0,
								 NULL,
								 OPEN_EXISTING,
								 FILE_ATTRIBUTE_NORMAL,
								 NULL);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		printf("Failed to obtain file handle to device: %s with Win32 Error code: %d\n",
			   "MyDDKDevice", GetLastError());
		return 1;
	}

	UCHAR buffer[10] = {};
	ULONG ulRead = 0;
	BOOL bRet = ReadFile(hDevice, buffer, 10, &ulRead, NULL);
	if (bRet)
	{
		printf("Read %d byte: ", ulRead);
		for (int i = 0; i < (int)ulRead; i++)//>
		{
			printf("%02X ", buffer[i]);
		}
		printf("\n");
	}

	CloseHandle(hDevice);

	system("pause");
	return 0;
}
```

简单例子作为参考，详细的可以参考《Windows驱动开发技术详解》中的例子，这里不再一一抄写例子。

**三种缓冲区使用方式**

前面的驱动例子中创建设备时，设备读写类型全都设置为`DO_BUFFERED_IO`，这是设备对象的读写方式。有三种读写方式如下：

1. 缓冲区方式读写，将设备类型设置为`DO_BUFFERED_IO`。
2. 直接读写方式，将设备类型设置为`DO_DIRECT_IO`。
3. 其他读写方式，将设备类型设置为0。

之所以有这三种读写方式其实是应用层如何和驱动交互数据决定的。

**缓冲区方式读写设备**，操作系统将用户模式提供的缓存区复制到内核模式地址下，这个内核中的buffer的地址由IRP的`pIrp->AssociatedIrp.SystemBuffer`成员指向。至于应用层传递下来的参数，则是由当前的`IO_STACK_LOCATION`中的内容指向。当IRP请求结束时，这段内核中的buffer会被复制到`ReadFile`或`WriteFile`提供的用户层Buffer中。

以缓冲区方式无论是读还是写设备，都会发生用户模式地址与内核模式地址的数据复制。复制的过程由操作系统负责，用户模式地址下的缓存由ReadFile或WriteFile函数调用者提供，而内核模式地址由操作系统负责分配和回收。

**直接方式读写设备**，操作系统会将用户模式下的缓存区锁住，然后操作系统将这段缓存区在内核模式地址再映射一遍，这样用户模式的缓冲区和内核模式的缓冲区指向了统一区域的物理内存。这样进程空间切换，也影响不到驱动的读写了。这种方式少了内存复制的开销，但是多出了地址映射。

在内核中可以通过`pIrp->MdlAddress`来获取应用层的虚拟内存的信息，NTDDK已经定义了如下几个宏可以帮助获取信息。

```
MmGetMdlVirtualAddress(Mdl);	// 获取要读写的用户层虚拟内存地址
MmGetMdlByteCount(Mdl);         // 获取读取的字节
MmGetMdlByteOffset(Mdl);        // 读取读写R3内存地址与页基址之间的偏移量
```

这种方式需要使用系统函数将用户层的虚拟地址所对应物理内存在内核中再进行一次映射，如下代码块所示，使用`MmGetSystemAddressForMdlSafe`可以将用户层虚拟地址对应物理内存在内核中再映射一次。

```
// 获取MDL在内核模式下的映射地址，可以直接使用
PVOID kernelAddr = MmGetSystemAddressForMdlSafe(pIrp->MdlAddress, NormalPagePriority);
```

**其他读写操作**，这种方式下派遣函数直接读写应用程序提供的缓存区地址，在驱动中直接读取应用程序的缓存区地址是很危险的，只有当驱动程序与应用程序运行在相同的线程上下文是才可以使用这种情况。应用层缓存区地址是通过`pIrp->UserBuffer`指针指向的。

由于直接读取用户层缓存很危险，所以在读写之前一定要判定内存的有效性，这可以使用`ProbeForWrite`等函数来检查，比如如下的形式。

```
__try{

	ProbeForWrite(pIrp->UserBuffer, ulReadLength, 4);

    // 不发生异常再操作内存。
    XXXXXXXX;
}
__except(EXCEPTION_EXECUTE_HANDLER)
{
	status = STATUS_UNSUCCESSFUL;
}
```

`I/O`设备控制操作中也涉及缓存的使用，其实和上述三种方式一样。

```
{
	// 输入输出缓存长度获取
	PIO_STACK_LOCATION pIOStack = IoGetCurrentIrpStackLocation(pIrp);
	pIOStack->Parameters.Read.Length;
	pIOStack->Parameters.Write.Length;
	pIOStack->Parameters.DeviceIoControl.InputBufferLength;
	pIOStack->Parameters.DeviceIoControl.OutputBufferLength;

	// 缓存方式获取读写缓存
	pIrp->AssociatedIrp.SystemBuffer;

	// 直接I/O方式获取内存
	pIrp->MdlAddress;
	LPVOID lpBuffer = MmGetSystemAddressForMdlSafe(pIrp->MdlAddress, NormalPagePriority);

	// 其他方式，直接读写R3层内存
	pIrp->UserBuffer
}
```

By Andy@2018-11-29 10:22:36