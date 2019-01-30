
#《Win驱动-从入门到入坑》笔记#

在`Bilibili`上有一套Windows驱动开发的视频，地址如下。这里看了一下这套视频，做了个笔记。

视频地址：[https://www.bilibili.com/video/av26193169/](https://www.bilibili.com/video/av26193169/)。

### 基础一 ###

如何编译第一个驱动？首先安装环境，安装VS2013或之后的VS版本，它们都集成了驱动开发，不需要再使用命令行编译。再一个需要安装的是`WDK8.1`或高版本，先安装VS，再安装WDK。

驱动使用C语言编写，当然也可以使用`C++`，但是会比较复杂。第一课以简单明了，尽量上手编写出可执行驱动为要。

驱动代码最简单形式如下，加载后直接返回成功：

```
#include <ntddk.h>

NTSTATUS DriverEntry(PDRIVER_OBJECT lpDrvObj, PUNICODE_STRING lpRegPath)
{
	return STATUS_SUCCESS;
}
```

但是上述代码是有问题的，首先是这个驱动可以被加载，但是加载后无法卸载，这是由于缺少卸载函数。再者，代码其实没什么用，一点逻辑也没有。

完善代码如下，编译出的驱动就可以正常加载与卸载，在启动和卸载时都有调试信息输出。

```
#include <ntddk.h>

NTSTATUS DriverUnload(PDRIVER_OBJECT lpDrvObj)
{
	KdPrint(("Driver Unload 0x%08X", lpDrvObj));
	return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT lpDrvObj, PUNICODE_STRING lpRegPath)
{
	lpDrvObj->DriverUnload = DriverUnload;
	KdPrint(("RegPath: %ws", lpRegPath->Buffer));
	return STATUS_SUCCESS;
}
```

`PDRIVER_OBJECT`定义如下，注释中有相应的解释。

```
typedef struct _DRIVER_OBJECT {
    CSHORT Type;    // 驱动类型
    CSHORT Size;    // 结构体大小

    PDEVICE_OBJECT DeviceObject; // 该驱动的设备对象，后面会有解释
    ULONG Flags;

    PVOID DriverStart;  // 驱动的入口函数，这个并不是前面DriverEntry，后面有讲
    ULONG DriverSize;   // 驱动模块大小
    PVOID DriverSection;// 这个后面讲解驱动隐藏时会讲解
    PDRIVER_EXTENSION DriverExtension; // 驱动扩展

    UNICODE_STRING DriverName; // 驱动名称，用于内核中标识驱动，其他模块调用
    PUNICODE_STRING HardwareDatabase;

    PFAST_IO_DISPATCH FastIoDispatch;
    PDRIVER_INITIALIZE DriverInit;
    PDRIVER_STARTIO DriverStartIo;
    PDRIVER_UNLOAD DriverUnload;   // 驱动卸载函数，在卸载驱动时会调用
    PDRIVER_DISPATCH MajorFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];//派遣函数
} DRIVER_OBJECT;
typedef struct _DRIVER_OBJECT *PDRIVER_OBJECT;
```

设备驱动程序，需要操作设备，具有分发函数，处理IRP等。内核模块中前面说的这些都不是必须的，如前面例子，它其实就是一个内核模块。

在内核中编程一定要小心谨慎，否则很容易造成蓝屏！！！

> 注：我使用VS2017编译的驱动与应用程序，它创建的默认工程编译出来的驱动直接在XP上测试会引发蓝屏。这里需要进行特殊设置，在`工程选项`设置中选择`C/C++`，然后选择`Code Generation`，然后修改`Security Check`选项，将它改为：`Disable Security Check (/GS-)`；其次修改`Driver Setting`中的`General`选项，将其中的`Target OS Version`选择为`Win7`，将`Target Platform`选择为`Desktop`。这样编译出来的驱动就可以在XP上用了。

> *X64系统上过签名验证*: 可以扒一个过期的签名，然后给驱动签上！这里将时间调到签名有效期内签名，然后再将时间改回来就可以了，这样就可以用于过系统的签名验证了。

### 基础二 ###

设备管理是分层的，比如网卡，应用层首先将数据传递到内核，内核中又有`I/O`管理层，协议层，Mini端口驱动，HAL层，最后到物理设备上。分层用于解决兼容问题，可以支持多种设备；分层解决复杂问题，每层有自己的功能。

如果驱动操作设备，那就是硬件驱动；否则它就是内核模块。

> X64上有PatchGuard，对于Win7，Win8以及早些的Win10等都已经有代码可以过这个防护机制了。

首先要创建设备对象，其类型如下，字段含义见注释。

```
typedef struct _DEVICE_OBJECT {
    CSHORT Type;     // 类型，USB，串口等
    USHORT Size;     // 结构体大小
    LONG ReferenceCount;
    struct _DRIVER_OBJECT *DriverObject; // 驱动对象，即DriverEntry参数
    struct _DEVICE_OBJECT *NextDevice;   // 设备对象水平链
    struct _DEVICE_OBJECT *AttachedDevice; // 挂接的设备对象，即当前对象的下层
    struct _IRP *CurrentIrp;
    PIO_TIMER Timer;
    ULONG Flags;             // See DO_... 缓冲方式
    ULONG Characteristics;   // See FILE_...
    __volatile PVPB Vpb;
    PVOID DeviceExtension;   // 设备扩展，自己的存储空间，特定于自己的数据
    DEVICE_TYPE DeviceType;  // 设备的类型
    CCHAR StackSize;         // 设备栈大小
    union {
        LIST_ENTRY ListEntry;
        WAIT_CONTEXT_BLOCK Wcb;
    } Queue;
    ULONG AlignmentRequirement;
    KDEVICE_QUEUE DeviceQueue;
    KDPC Dpc;

    //
    //  如下的字段是文件系统互斥使用的，用于追踪当前正是用当前对象的Fsp线程
    //
    ULONG ActiveThreadCount;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    KEVENT DeviceLock;

    USHORT SectorSize;
    USHORT Spare1;

    struct _DEVOBJ_EXTENSION  *DeviceObjectExtension;
    PVOID  Reserved;

} DEVICE_OBJECT;
```

调用`IoCreateDevice`函数为驱动创建设备对象，详细参数可以参考WDK文档。调用`IoCreateSymbolicLink`为设备对象创建链接，用于R3层调用驱动时使用。

> `Rtl*`为运行时函数，内核提供给驱动程序使用。

创建完设备对象以及设备对象的链接，然后设置设备对象的缓冲形式（设置为`DO_BUFFERED_IO`，即缓存`I/O`方式），设置它的派遣函数等，这里以读数据为例。

要响应读请求，则需为驱动设置读请求处理函数，即`MajorFunction`中的`IRP_MJ_READ`对应的项。在读请求处理函数中，首先获取使用`IoGetCurrentIrpStackLocation`当前设备对象在IRP中对应的堆栈，从堆栈中可以取得调用参数，比如要读取的数据长度。将要读取的数据内容设置写到缓存（这里是将整个缓存设置为`0xAA`），然后完成IRP，这需设置IRP中的`IoStatus`结构体成员的值，并调用`IoCompleteRequest`函数完成这个IRP。

到这里其实从驱动中读取数据的基本代码就写完了，但是由于读取数据首先要打开设备，所以要添加打开（`IRP_MJ_CREATE`）的处理函数；在完成读操作之后R3会将打开句柄关闭，所以要添加关闭的处理函数（`IRP_MJ_CLOSE`）。如果不设置这个创建处理函数，在R3打开设备句柄就会失败。再者，在初始化驱动时创建了设备对象用于R3使用，在卸载驱动时则需要将创建的对象删除掉，否则会出现问题。

完整代码如下所示：

```
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
```

R3的读取代码如下所示，这个代码比较简单，没什么可解说。

```
#include <stdlib.h>
#include <Windows.h>

int _tmain(int argc, TCHAR* argv[])
{
	HANDLE hDevice =
		CreateFileW(L"\\\\.\\MyRead",
			GENERIC_READ| GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		printf("Failed to obtain file handle to devices: Error %d\n", GetLastError());
		system("pause");
		return 1;
	}

	UCHAR buffer[10];
	ULONG ulRead;
	BOOL bRet = ReadFile(hDevice, buffer, 10, &ulRead, NULL);
	if (bRet)
	{
		printf("Read %d bytes: ", ulRead);
		for (int i = 0; i < (int)ulRead; i++)
		{
			printf("%02X ", buffer[i]);
		}
		printf("\n");
	}
	CloseHandle(hDevice);
	system("pause");

    return 0; //>
}
```

### 基础三 ###

**自定义驱动通信**

首先定义一个通信码，用于数据分发。

```
#define IOCTL_TEST1 CTL_CODE(\
			FILE_DEVICE_UNKNOWN,\	// 设备种类
			0x800,\					// Code，消息码
			METHOD_BUFFERED,\		// 读写方式，缓冲
			FILE_ANY_ACCESS)		// 非独占
```

其实可以在上一节中的代码中直接修改，不过由于是自定义的通信，不能使用`IRP_MJ_*`主功能号。在其中提供了一个`IRP_MJ_DEVICE_CONTROL`，它在R3层体现为对`DeviceIoControl`函数调用的响应，这个函数调用用于`I/O`控制中的杂类，可以自定义“通信协议”，下面的代码就是通过这个功能实现。

为`IRP_MJ_DEVICE_CONTROL`提供一个独立的派遣函数，用于处理我们自定协议的数据分发。如下`MyDriverIoctl`函数所示。首先还是要获取当前IRP在`I/O`栈中对应的内容`PIO_STACK_LOCATION`，然后从中获取R3层调用下来时的参数，这次与读不同，位于`Parameters.DeviceIoControl.*`中，其中`Parameters.DeviceIoControl.IoControlCode`中包含了从`DeviceIoControl`调用时传递进来的协议号。剩余的读写内容与普通的读写类似，如代码所示。

```
#include <ntddk.h>

#define IOCTL_TEST1 CTL_CODE(\
			FILE_DEVICE_UNKNOWN,\
			0x800,\
			METHOD_BUFFERED,\
			FILE_ANY_ACCESS)

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

NTSTATUS DispatchFunction(PDEVICE_OBJECT pDevice, PIRP pIrp)
{
	UNREFERENCED_PARAMETER(pDevice);

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS MyDriverIoctl(PDEVICE_OBJECT pDevice, PIRP pIrp)
{
	UNREFERENCED_PARAMETER(pDevice);
	NTSTATUS status = STATUS_SUCCESS;
	PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(pIrp);

	ULONG info = 0;
	ULONG inLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
	ULONG outLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

	ULONG uCode = irpStack->Parameters.DeviceIoControl.IoControlCode;
	switch (uCode)
	{
		case IOCTL_TEST1:
		{
			KdPrint(("self defined IO\n"));
			PUCHAR inBuffer = pIrp->AssociatedIrp.SystemBuffer;
			for (ULONG i = 0; i < inLength; i++)
			{
				KdPrint(("%02x", inBuffer[i]));
			}
			PUCHAR outBuffer = pIrp->AssociatedIrp.SystemBuffer;
			memset(outBuffer, 0, outLength);
			memset(outBuffer, 0xBB, outLength / 2);
			info = outLength / 2;
			break;
		}
		default:
		{
			KdPrint(("failed"));
			status = STATUS_UNSUCCESSFUL;
			break;
		}
	}

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = info;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
	PDEVICE_OBJECT pDevice = NULL;
	KdPrint(("Driver Reg: %ws", reg_path->Buffer));

	// 派遣函数，类似R3的消息循环
	driver->DriverUnload = Unload;
	for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		driver->MajorFunction[i] = DispatchFunction;
	}
	driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MyDriverIoctl;

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
```

R3的控制代码也比较简单，直接调用`DeviceIoControl`函数，传入我们自定义的协议号即可。

```
#include "stdafx.h"
#include <stdlib.h>
#include <Windows.h>
#include <WinIoCtl.h>  // IoCtl 操作必须的头文件

#define IOCTL_TEST1 CTL_CODE(\
			FILE_DEVICE_UNKNOWN,\
			0x800,\
			METHOD_BUFFERED,\
			FILE_ANY_ACCESS)

int _tmain(int argc, TCHAR* argv[])
{
	HANDLE hDevice =
		CreateFileW(L"\\\\.\\MyRead",
			GENERIC_READ| GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		printf("Failed to obtain file handle to devices: Error %d\n", GetLastError());
		system("pause");
		return 1;
	}

	DWORD dwOutput = 0;
	UCHAR InputBuffer[10];
	UCHAR OutputBuffer[10];
	memset(OutputBuffer, 0, 10);
	memset(InputBuffer, 0xaa, 10);

	BOOL bRet = DeviceIoControl(hDevice, IOCTL_TEST1, InputBuffer, 10, &OutputBuffer, 10, &dwOutput, NULL);
	if (bRet)
	{
		printf("Output Buffer %d bytes: ", dwOutput);
		for (int i = 0; i < (int)dwOutput; i++)
		{
			printf("%02X ", OutputBuffer[i]);
		}
		printf("\n");
	}
	CloseHandle(hDevice);
	system("pause");

    return 0;
}//>
```

**杀死进程**

驱动其实杀死一个进程比较简单，只需要将进程ID传给内核模块，从内核模块中打开进程，调用系统函数`ZwTerminateProcess`即可，如下两段代码是相对于前面的通信代码编写，添加即可。

```
	......
	long pid = 0;
	printf("please enter the process pid:\n");
	scanf_s("%d", &pid);

	BOOL bRet = DeviceIoControl(hDevice, IOCTL_TEST1, &pid, 4, &OutputBuffer, 10, &dwOutput, NULL);
    ......
```

```
BOOLEAN MyKillProcess(LONG pid)
{
	HANDLE ProcessHandle;
	NTSTATUS status;
	OBJECT_ATTRIBUTES ObjectAttributes;
	CLIENT_ID myCid;

	InitializeObjectAttributes(&ObjectAttributes, 0, 0, 0, 0);

	myCid.UniqueProcess = (HANDLE)pid;
	myCid.UniqueThread = 0;

	status = ZwOpenProcess(
		&ProcessHandle,
		PROCESS_ALL_ACCESS,
		&ObjectAttributes,
		&myCid);
	if (NT_SUCCESS(status))
	{
		ZwTerminateProcess(ProcessHandle, status);
		ZwClose(ProcessHandle);
		return TRUE;
	}

	return FALSE;
}
```

> 注：这是使用系统基本函数杀死进程，这个很容易被绕过，比如PCHunter有自我保护，使用这基础方法就没那么容易将它杀掉了。

###双机调试与内存分配###

**双机调试**

视频中作者使用了VS2013内置的调试器链接的虚拟机，然后进行调试。其实还可以使用`Windbg`和`VirtualKD`来进行Windows内核调试。

```
kd> dv		// 显示当前栈帧的局部变量

kd> db      // 以字节形式显示指定地址数据

kd> dd      // 以四字节形式显示指定地址数据

kd> u 0xXXXXXXXX // 反汇编指定地址

kd> uf 0xXXXXXXXX // 反汇编地址处的函数
```

调试命令有很多，可以学习一下调试器，或用到时查阅资料即可。

**内存分配**

机器本身的物理内存只有六百兆，而进程本身的内存空间有4G。要实现这样的功能，就需要系统的虚拟内存机制。R3层应用程序之间是相互隔离的，同一个内存地址在不同的进程中对应的内容并不相同，这也有赖于虚拟内存机制。

在内核编程中，内存分为换页内存（Paged Memory）和非换页内存（None Paged Memory）。非分页内存用于优先级高于线程调度级别的功能，这些功能中无法响应缺页异常（缺页异常优先级低，无法抢占这些高优先级异常），导致执行失败（炸了）。

在驱动中分配内存很容易，如下的三种：分页内存，非分页内存，带有标记的分页和非分页内存。

```
	......

	testPool = ExAllocatePool(PagedPool, 10);
	if (testPool != NULL)
	{
		memset(testPool, 0xbb, 10);
		for (size_t i = 0; i < 10; i++)//>
		{
			KdPrint(("%02x", *((PCHAR)testPool + i)));
		}
		ExFreePool(testPool);
	}

	noneTestpool = ExAllocatePool(NonPagedPool, 10);
	if (noneTestpool != NULL)
	{
		memset(noneTestpool, 0xcc, 10);
		for (size_t i = 0; i < 10; i++) //>
		{
			KdPrint(("%02x", *((PCHAR)noneTestpool + i)));
		}
		ExFreePool(noneTestpool);
	}
	noneTestpool = ExAllocatePoolWithTag(NonPagedPool, 10, 'nopg');
	if (noneTestpool)
	{
		ExFreePool(noneTestpool);
	}
    ......
}
```

### 键盘过滤驱动 ###

过滤驱动是内核开发中比较重要的概念，这一节介绍这个内容。Windows中的驱动设备都是分层的，比如网络中可能有如下的一个分层。

| 应用层Send/Recv  |
|-----------------|
|       TDI       |
|    TCP/UDP协议   |
| 虚拟设备（端口协议）|
|     物理网卡      |

例如抓包软件`Wireshark`，它在`TCP/UDP`中间插入一层协议，绑定网卡，网卡会将数据同时转发Wirshark和`TCP/UDP`协议层。

另外一种过滤是在设备栈中插入过滤设备，将驱动的设备对象绑定到目标设备上，这样从设备栈就可以将上下行数据进行截取！

`csrss.exe`进程负责处理键盘按键内容，并且负责将数据分发到相应的程序。按下键盘按键后，键盘要等待某个驱动或进程来读取按键内容，而不是主动地自下而上传输按键。按下Key，比如`A`，这时键盘会产生一个硬件中断，系统会捕捉到中断，从而得知键盘上有了操作，然后系统开始层层往下去读取键盘按键内容。

对于绑定设备栈中的某个设备时，当前设备和被绑定设备并非上下层关系，这个当前设备其实是放在整个设备栈的顶端。比如，要绑定到`Device 2`，那当前设备对象就会被放到`Device 1`之上，而非`Device 2`和`Device 1`之间。

| Device 1 |
|----------|
| Device 2 |
| Device 3 |
| Device 4 |

`ObReferenceObjectByName()`和`ObDereferenceObject()`两个函数用于获取内核中的对象并增减引用。

```

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
```

驱动需要处理电源与即插即用的事件，但是只是将对应IRP进行转发。当有读取键盘的操作时，设置IRP完成回调，当读取了相应的键值时，回调我们的函数，这样就可以对获取的数据进行过滤了。

###FSD钩子和线程###

**FSD钩子**

在驱动中Hook是一门比较有用的技术，比较有名的一种就是FSD钩子。在X64下，Win7，Win8不会触发PG，Patchguard系统只保护ntos模块，而在Win10上这种方式就已经失效了。

如下代码为例子代码，例子代码并不完善，只是用于说明问题。其中挂钩子时需要使用原子操作或提高系统级别，防止替换过程中出现问题。

```
#include <ntddk.h>

extern POBJECT_TYPE* IoDriverObjectType;

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

typedef NTSTATUS(*POriginRead)(PDEVICE_OBJECT pdevice, PIRP pirp);

POriginRead OriginRead = NULL;

NTSTATUS Unload(PDRIVER_OBJECT driver)
{
	UNREFERENCED_PARAMETER(driver);
	KdPrint(("this driver is unloading...\n"));

	if (NULL != OriginRead)
	{
		driver->MajorFunction[IRP_MJ_READ] = OriginRead;
	}

	return STATUS_SUCCESS;
}

NTSTATUS myreadpatch(PDEVICE_OBJECT device, PIRP irp)
{
	KdPrint(("-----read-----"));

	return OriginRead(device, irp);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
	KdPrint(("Driver Reg: %ws", reg_path->Buffer));

	driver->DriverUnload = Unload;

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

	OriginRead = (POriginRead)kbddriver->MajorFunction[IRP_MJ_READ]; // 最好用原子操作，防止出现问题
	kbddriver->MajorFunction[IRP_MJ_READ] = myreadpatch;             // 或者提升中断级
	return STATUS_SUCCESS;
}
```

**内核中的线程**

内核线程不同于R3的线程，在内核中有专门的函数用于创建它（`PsCreateSystemThread`）；不同于R3的线程，内核线程不会自己终止，需要在线程退出之前调用`PsTerminateSystemThread`函数将系统线程终止掉。再者就是在内核线程中一定要调用`KeDelayExecutionThread`进行延时，否则一旦进入死循环而又没有延时，会造成CPU被这个系统线程耗尽。

再一点需要注意是驱动卸载的处理，由于在线程中存在休眠两秒的逻辑，所以卸载驱动时需要等待线程退出，`KeWaitForSingleObject`用于等待一个可等待对象。由于卸载函数逻辑执行很快，并且一旦卸载逻辑完成，内存中的PE文件就会被释放掉，如果不等待线程退出就释放掉PE占据的内存，那么线程再执行就会出现无效内存，造成蓝屏。

```
#include <ntddk.h>

extern POBJECT_TYPE *PsThreadType;

BOOLEAN bTerminated = FALSE;
PETHREAD pThreadObj = NULL;

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

NTSTATUS Unload(PDRIVER_OBJECT driver)
{
	UNREFERENCED_PARAMETER(driver);
	KdPrint(("this driver is unloading...\n"));
	bTerminated = TRUE;
	KeWaitForSingleObject(pThreadObj, Executive, KernelMode, FALSE, NULL);
    ObDereferenceObject(pThreadObj);

	return STATUS_SUCCESS;
}

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

	CreateMyThread(NULL);

	return STATUS_SUCCESS;
}
```

其中的休眠逻辑其实可以自己封装一个函数，比如Sleep，用于调用后等待几秒。代码如下所示，其与前面线程中的等待逻辑并没有什么区别。

```
void Sleep(DWORD dwSecs)
{
	LARGE_INTEGER interval;
	interval.QuadPart = -10000000*dwSecs;
	KeDelayExecutionThread(KernelMode, FALSE, &interval);
}
```

###InlineHook实现驱动级变速###

这一节实现一个驱动级的变速，就是通过挂接时间函数钩子，修改时间变化速率。

这一节需要使用Inline Hook来实现，具体的InlineHook内容可以参考detours或其他类似的钩子库的实现。简单说就是修改被Hook函数的头部，跳转到我们的函数上，执行完自己逻辑之后，再跳转回原函数继续执行。

如下为可执行完整代码：

```
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
```

至于变速齿轮是什么也没有太弄明白，游戏外挂中有变速齿轮的使用。

###内核回调函数###



###X86逆向基础###

###驱动实现本地时间验证###

###驱动模块隐藏###

###一种特殊钩子###


###进程隐藏###

###Pubg-PakHacker###

###反虚拟机###

###手写内核Shellcode###



By Andy@2019-01-10 12:17:32