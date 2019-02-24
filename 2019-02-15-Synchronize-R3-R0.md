#同步#

###R3的同步###

在Ring3使用的同步机制都是内核提供的，主要包括事件，信号量，互斥体。这三个有各自的创建函数，获取它们则是使用相同的函数`WaitForSingleObject()`或`WaitForMultipleObjects()`，它们的使用示例如下代码段所示。

```
#include <windows.h>
#include <process.h>
#include <stddef.h>
#include <stdlib.h>
#include <conio.h>

UINT WINAPI	ThreadEvent(LPVOID para)
{
	printf("Enter ThreadEvent!\n");

	HANDLE *pEvent = (HANDLE*)para;
	if (NULL != pEvent)
		SetEvent(*pEvent);

	printf("Leave ThreadEvent!\n");
	return 0;
}

UINT WINAPI	ThreadSemaphere(LPVOID para)
{
	printf("Enter ThreadSemaphere!\n");
	HANDLE *pSemaphore = (HANDLE*)para;

	Sleep(2000);

	ReleaseSemaphore(*pSemaphore, 1, NULL);

	printf("Leave ThreadSemaphere!\n");
	return 0;
}

UINT WINAPI	ThreadMutex1(LPVOID para)
{
	printf("Enter ThreadMutex1\n");
	HANDLE *pMutex = (HANDLE*)para;
	if (pMutex)
	{
		WaitForSingleObject(*pMutex, INFINITE);

		WaitForSingleObject(*pMutex, INFINITE);

		printf("ThreadMutex1 Wait Two Time Mutex\n");

		Sleep(3000);
		printf("ThreadMutex1 Sleep comeback\n");

		ReleaseMutex(*pMutex);
	}
	printf("Leave ThreadMutex1\n");

	return 0L;
}

UINT WINAPI	ThreadMutex2(LPVOID para)
{
	printf("Enter ThreadMutex2\n");
	HANDLE *pMutex = (HANDLE*)para;
	if (pMutex)
	{
		printf("ThreadMutex2 Wait One Time Mutex Before\n");
		WaitForSingleObject(*pMutex, INFINITE);
		printf("ThreadMutex2 Wait One Time Mutex After\n");

		Sleep(3000);
		printf("ThreadMutex2 Sleep comeback\n");

		ReleaseMutex(*pMutex);
	}
	printf("Leave ThreadMutex2\n");

	return 0L;
}


int _tmain(int argc, _TCHAR* argv[])
{
	//----------------------------------------
	// Event
	HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	HANDLE hThreadEvent = (HANDLE)_beginthreadex(NULL, 0, ThreadEvent, &hEvent, 0, NULL);
	if (hThreadEvent)
		CloseHandle(hThreadEvent);

	WaitForSingleObject(hEvent, INFINITE);

	//----------------------------------------
	// Semaphore
	HANDLE hSemaphore = CreateSemaphore(NULL, 2, 2, NULL);
	if (hSemaphore)
	{
		WaitForSingleObject(hSemaphore, INFINITE);
		WaitForSingleObject(hSemaphore, INFINITE);
		HANDLE hThreadSemaphore = (HANDLE)_beginthreadex(NULL, 0, ThreadSemaphere, &hSemaphore, 0, NULL);
		if (hThreadSemaphore)
			CloseHandle(hThreadSemaphore);

		WaitForSingleObject(hSemaphore, INFINITE);
		printf("Main Thread get Semaphore Signal\n");
	}

	//-----------------------------------------
	// Mutex
	HANDLE hMutex = CreateMutex(NULL, FALSE, NULL);
	if (hMutex)
	{
		HANDLE hThreadMutex1 = (HANDLE)_beginthreadex(NULL, 0, ThreadMutex1, &hMutex, 0, NULL);
		HANDLE hThreadMutex2 = (HANDLE)_beginthreadex(NULL, 0, ThreadMutex2, &hMutex, 0, NULL);

		HANDLE hThreads[2] = {};
		hThreads[0] = hThreadMutex1;
		hThreads[1] = hThreadMutex2;
		WaitForMultipleObjects(2, hThreads, TRUE, INFINITE);
		CloseHandle(hThreads[0]);
		CloseHandle(hThreads[1]);
	}

	system("pause");

	return 0;
}
```

除了上述的这些方法外，在Ring3进行线程的“同步”或“互斥”还可以使用临界区（`CRITICAL_SECTION`），它只适用于进程内的线程之间的同步。`Interlocked*`系列函数则可以用于基本变量的互斥修改。

###R0的同步###

**自旋锁**

它是一种同步处理机制，能够保证某个资源只能被一个线程所拥有。自旋锁如其名字含义，如果有程序申请获取的自旋锁被锁住了，那么这个线程就进入了自旋状态。所谓自旋状态就是不停询问是否可以获取自旋锁。

它与等待事件不同，自旋状态不会让当前线程进入休眠，而是一直处于自旋，这不太适合与长时间占用锁的逻辑。

```
KSPIN_LOCK MySpinLock;  // 定义自旋锁对象，一般要定义为全局的。

KIRQL oldirql;
KeAcquireSpinLock(&MySpinLock, &oldirql);

KeReleaseSpinLock(&MySpinLock, &oldirql);
```

**内核同步**

在内核中也有两个函数负责等待内核同步对象，与Ring3层的两个函数类似，它们是`KeWaitForSingleObject()`和`KeWaitForMultipleObject()`。

内核中创建线程的函数为`PsCreateSystemThread()`，其参数`ProcessHandle`表示线程的归属，如果这个参数为NULL，那么创建的线程是系统线程，从属于`Idle`进程；如果值为进程句柄，则创建的线程归属于该进程。

*内核模式下的事件对象*

`KeInitializeEvent()`函数用于对事件对象初始化，事件对象有两种类型，一种是通知事件，其类型为`NotificationEvent`；另一类为同步事件，对应类型参数为`SynchronizationEvent`。对于通知事件，当事件对象变为激发状态时，则需要手动将其改回为激发状态；对于同步事件变为激发状态后，遇到`KeWaitForXX`等内核函数，事件对象自动变回为激发状态。这其实就对应于R3的自动和手动设置状态的情况。

对于R3层的事件可以将句柄传递到驱动中，然后使用DDK提供的`ObReferenceObjectByHandle`获取内核对象指针，然后对事件对象进行操作。

如下代码中，通过线程给出了创建和设置事件的例子。在后面则通过`DeviceIoControl()`将事件句柄传递给驱动，在驱动中操作事件。

```
#pragma PAGECODE
VOID EventTestThread(IN PVOID pContext)
{
	PKEVENT pEvent = (PKEVENT)pContext;
	KdPrint(("Enter EventTestThread\n"));
	if (pEvent)
	{
		KeSetEvent(pEvent, IO_NO_INCREMENT, FALSE);
	}
	KdPrint(("Leave EventTestThread\n"));
	PsTerminateSystemThread(STATUS_SUCCESS);
}

#pragma PAGECODE
VOID EventTest()
{
	HANDLE hMyThread;
	KEVENT hEvent;

	KeInitializeEvent(&hEvent, NotificationEvent, FALSE);
	NTSTATUS status = PsCreateSystemThread(&hMyThread, 0, NULL, NtCurrentProcess(), NULL, EventTestThread, &hEvent);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Create Process Thread Error!\n"));
	}

	KeWaitForSingleObject(&hEvent, Executive, KernelMode, FALSE, NULL);
	KdPrint(("Event has Single\n"));
}

#pragma PAGECODE
VOID SetUserEvent(HANDLE hEvent)
{
	if (hEvent == NULL)
		return;

	PKEVENT pEvent = NULL;
	NTSTATUS Status = ObReferenceObjectByHandle(hEvent, EVENT_MODIFY_STATE, *ExEventObjectType, KernelMode, (PVOID*)&pEvent, NULL);
	if (NT_SUCCESS(Status) && pEvent != NULL)
	{
		KeSetEvent(pEvent, IO_NO_INCREMENT, FALSE);
		ObDereferenceObject(pEvent);
	}
}

#pragma PAGECODE
NTSTATUS HelloDDKIoCtlRoutine(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
	KdPrint(("Enter HelloDDKIoCtlRoutine DevObj: %p\n", pDevObj));
	NTSTATUS status = STATUS_SUCCESS;

	PIO_STACK_LOCATION pIoStack = IoGetCurrentIrpStackLocation(pIrp);
	//ULONG cbInBuffer = pIoStack->Parameters.DeviceIoControl.InputBufferLength;
	//ULONG cbOutBuffer = pIoStack->Parameters.DeviceIoControl.OutputBufferLength;

	ULONG ctlCode = pIoStack->Parameters.DeviceIoControl.IoControlCode;
	switch (ctlCode)
	{
	case ID_IOCTL_TEST1:
	{
		KdPrint(("Create Thread: \n"));
		CreateThread_Test();

		KdPrint(("Event Test: \n"));
		EventTest();
	}
	case ID_IOCTL_TRANSMIT_EVENT:
	{
		KdPrint(("Start Thread Set User Event:"));
		HANDLE hEvent = (HANDLE)*(LONG_PTR*)pIrp->AssociatedIrp.SystemBuffer;
		SetUserEvent(hEvent);
	}
	default:
		break;
	}

	// 完成IRP
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	KdPrint(("Leave HelloDDKIoCtlRoutine\n"));
	return status;
}
```

还有函数`IoCreateNotificationEvent()`和`IoCreateSynchronizationEvent()`用于创建命名的通知事件和同步事件。

**内核信号灯**

内核的信号灯也类似R3的信号灯。`KeInitializeSemaphore()` 用于初始化信号灯对象。`KeReadStateSemaphore()`函数可以读取信号灯当前的计数，`KeReleaseSemaphore()`用于增加信号灯计数。

使用信号灯的一个例子如下代码所示。

```
#pragma PAGECODE
VOID SemaphoreThread(IN PVOID pContext)
{
	PKSEMAPHORE pkSemaphore = (PKSEMAPHORE)pContext;
	KdPrint(("Enter Semaphore Thread\n"));
	if (pkSemaphore)
	{
		KeReleaseSemaphore(pkSemaphore, IO_NO_INCREMENT, 1, FALSE);
	}
	KdPrint(("Leave Semaphore Thread\n"));
	PsTerminateSystemThread(STATUS_SUCCESS);
}

#pragma PAGECODE
VOID SemaphoreTest()
{
	HANDLE hMyThread;
	KSEMAPHORE kSemaphore;

	KeInitializeSemaphore(&kSemaphore, 2, 2);
	LONG count = KeReadStateSemaphore(&kSemaphore);
	KdPrint(("The Semaphore count is %d\n", count));
	KeWaitForSingleObject(&kSemaphore, Executive, KernelMode, FALSE, NULL);
	count = KeReadStateSemaphore(&kSemaphore);
	KdPrint(("The Semaphore count is %d\n", count));
	KeWaitForSingleObject(&kSemaphore, Executive, KernelMode, FALSE, NULL);

	NTSTATUS status = PsCreateSystemThread(&hMyThread, 0, NULL, NtCurrentProcess(), NULL, SemaphoreThread, &kSemaphore);
	if (NT_SUCCESS(status))
	{
		ZwClose(hMyThread);
	}
	KeWaitForSingleObject(&kSemaphore, Executive, KernelMode, FALSE, NULL);
	KdPrint(("After KeWaitForSingleObject\n"));

	return ;
}
```

**内核互斥体**

内核中互斥体的使用于R3层类似，内核的数据结构为`KMUTEX`，使用前先初始化（`KeInitializeMutex()`）内核互斥体对象。如下代码段，给出了如何使用互斥体的简单例子：

```
VOID __stdcall MutexThread1(PVOID lpParam)
{
	KdPrint(("Enter MutexThread1\n"));
	PKMUTEX pKMutex = (PKMUTEX)lpParam;
	if (pKMutex)
	{
		KeWaitForSingleObject(pKMutex, Executive, KernelMode, FALSE, NULL);
		KdPrint(("MutexThread1 After Wait For Mutext!\n"));

		KeStallExecutionProcessor(50);
		KdPrint(("MutexThread1 After stall Execute!\n"));

		KeReleaseMutex(pKMutex, FALSE);
	}

	KdPrint(("Leave MutexThread1\n"));
	PsTerminateSystemThread(STATUS_SUCCESS);
}

VOID __stdcall MutexThread2(PVOID lpParam)
{
	KdPrint(("Enter MutexThread2\n"));
	PKMUTEX pKMutex = (PKMUTEX)lpParam;
	if (pKMutex)
	{
		KeWaitForSingleObject(pKMutex, Executive, KernelMode, FALSE, NULL);
		KdPrint(("MutexThread2 After Wait For Mutext!\n"));

		KeStallExecutionProcessor(50);
		KdPrint(("MutexThread2 After stall Execute!\n"));

		KeReleaseMutex(pKMutex, FALSE);
	}

	KdPrint(("Leave MutexThread2\n"));
	PsTerminateSystemThread(STATUS_SUCCESS);
}

#pragma PAGECODE
VOID MutexTest()
{
	HANDLE hMyThread1, hMyThread2;
	KMUTEX kMutex;
	KdPrint(("Enter MutexTest Func\n"));
	KeInitializeMutex(&kMutex, 0);

	NTSTATUS status = PsCreateSystemThread(&hMyThread1, 0, NULL, NtCurrentProcess(), NULL, MutexThread1, &kMutex);
	status = PsCreateSystemThread(&hMyThread2, 0, NULL, NtCurrentProcess(), NULL, MutexThread2, &kMutex);
	PVOID Pointer_Array[2];
	if (hMyThread1)
	{
		ObReferenceObjectByHandle(hMyThread1, 0, NULL, KernelMode, &Pointer_Array[0], NULL);
		ZwClose(hMyThread1);
	}
	if (hMyThread2)
	{
		ObReferenceObjectByHandle(hMyThread2, 0, NULL, KernelMode, &Pointer_Array[1], NULL);
		ZwClose(hMyThread2);
	}

	KeWaitForMultipleObjects(2, Pointer_Array, WaitAll, Executive, KernelMode, FALSE, NULL, NULL);
	ObDereferenceObject(Pointer_Array[0]);
	ObDereferenceObject(Pointer_Array[1]);

	KdPrint(("Leave MutexTest Func\n"));
}
```

**快速互斥体**

快速互斥体(Fast Mutex)是DDK提供的另外一种内核同步对象，与前面介绍普通互斥体类似，但是其执行速度比普通互斥体快，同时它也有一个缺点：无法递归获取互斥体对象。

与普通的互斥体不同，它使用`FAST_MUTEX`数据结构来描述快速互斥体对象，它有自己独特的函数用于获取和释放互斥体：`KeInitializeFastMutex()`用于初始化快速互斥体，`ExAcquireFastMutex()`用于获取快速互斥体，`KeReleaseFastMutex()`用于释放互斥体。

**其他的同步方法**

1. 自旋锁，前面简单介绍过用法，对于想要同步的某段代码区域，就可以使用自旋锁，它类似R3的临界区，同样也不适合于大段代码或具有延时的同步操作。在单CPU中自旋锁是通过提升IRQL实现，而在多CPU中实现方法则比较复杂。
2. 使用互锁操作进行同步，即`InterlockedXX`和`ExInterlockedXX`，`Ex*`系列函数需要提供自旋锁，所以它不能够操作分页内存。

###IRP的同步###

IRP是对设备的操作转化而来，一般情况下IRP由操作系统异步发送。异步处理IRP有助于提高效率，但是有时候异步处理会导致逻辑错误；这时候就需要将异步的IRP进行同步化。

**同步与异步操作原理**

操作设备的函数，比如`ReadFile()`和`DeviceIOControl()`等，如果是同步操作，那么在请求完成以前这个函数是不会返回；对于异步操作，一旦将请求传递给内核后，该函数马上返回，即这些函数调用完成后，请求任务并不一定完成。

同步操作时，在`DeviceIOControl()`等函数内部会调用`WaitForSingleObject`等函数去等待一个事件；这个事件会在IRP被结束时由`IoCompleteRequest()`函数来设置该事件。而异步操作中，`DeviceIOControl()`函数被调用时，其内部产生IRP并将它传递给驱动内部派遣函数；此时`DeviceIOControl()`函数并不等待该IRP结束，而是直接返回；当IRP经过一段时间结束时，操作系统出发IRP相关的事件，用于通知应用程序IRP请求结束。由此可见异步的操作比同步操作要更有效率。

在R3层要对设备进行异步操作，那么对于`CreateFile`的第六个参数`DWORD dwFlagAndAttributes`要设置`FILE_FLAG_OVERLAPPED`标志，如果不设置该参数，打开的设备句柄就不具备异步操作能力。

一旦在打开设备时设置了异步操作的标记，那么在操作设备时就可以使用异步特性。无论`ReadFile()`或是`DeviceIoControl()`等函数都包含一个参数`LPOVERLAPPED lpOverlapped`，它是用于异步操作必要结构，如下所示。

```
typedef struct _OVERLAPPED {
    ULONG_PTR Internal;
    ULONG_PTR InternalHigh;
    union {
        struct {
            DWORD Offset;
            DWORD OffsetHigh;
        } DUMMYSTRUCTNAME;
        PVOID Pointer;
    } DUMMYUNIONNAME;
    HANDLE  hEvent;	// 用于在该操作完成后通知应用程序，由IoCompleteRequest设置状态
} OVERLAPPED, *LPOVERLAPPED;
```

所以在调用设备操作函数时，初始化该结构体并传递给函数，只需要等待其中事件即可。

系统SDK还提供了另外一种用于异步操作的函数，`ReadFileEx`和`WriteFileEx`函数，他们专门用于异步读写操作，不能进行同步读写。

```
BOOL
WINAPI
ReadFileEx(
    HANDLE hFile,
    LPVOID lpBuffer,
    DWORD nNumberOfBytesToRead,
    LPOVERLAPPED lpOverlapped,
    LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

BOOL
WINAPI
WriteFileEx(
    HANDLE hFile,
    LPCVOID lpBuffer,
    DWORD nNumberOfBytesToWrite,
    LPOVERLAPPED lpOverlapped,
    LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
```

其中的`LPOVERLAPPED lpOverlapped`的用法与前面类似，参数`LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine`则是提供给内核回调的例程。Windows在完成了操作之后，插入一个APC用于调用这个回调函数。要让线程调用APC，那么需要将线程置于警惕（Alert）状态，多个API可以使线程进入静态状态，比如`SleepEx`，`WaitForSingleObjectEx`，`WaiForMutipleObjectEx`等函数，它们都有一个参数`BOOL bAlertable`用于设置线程进入警惕模式。

**IRP同步与异步完成**

IRP被异步完成即IRP不在派遣函数中调用`IoCompleteRequest()`内核函数，调用`IoCompleteRequest()`即意味着IRP请求结束，标志着本次对设备操作结束。

异步操作IRP只有两种形式，通过`ReadFile`发起的异步请求，当读取的派遣函数返回时并不调用函数完成此IRP，而是将IRP状态设置为`ERROR_IO_PENDING`，意味着ReadFile并没有真正完成；当IRP真正完成时即调用`IoCompleteRequest`函数设置overlap中的事件。对于`ReadFileEx`发起的具有回调的操作，当IRP真正结束时，调用`IoCompleteRequest`后则将提供的回调函数插入APC队列；如果线程进入了警惕状态则线程返回时即执行APC。

对于挂起的IRP则需要设置IRP的状态，同时派遣函数也需要返回挂起状态，如下代码所示：

```
NTSTATUS HelloDDKRead(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
	KdPrint(("Enter HelloDDKRead\n"));

	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	PMY_IRP_ENTRY pIrpEntry = (PMY_IRP_ENTRY)ExAllocatePool(PagedPool, sizeof(MY_IRP_ENTRY));
	pIrpEntry->pIrp = pIrp;
	InsertHeadList(pDevExt->pIrpLinkList, &pIrpEntry->ListEntry);

	IoMarkIrpPending(pIrp);

	KdPrint(("Leave HelloDDKRead\n"));

	return STATUS_PENDING;
}
```

当然对于挂起的IRP则一定在完成了操作只要要将其调用`IoCompleteRequest`函数完成。

对于挂起IRP除了将其完成外，另外一种操作是取消。取消IRP可以由R3层调用`CancelIO()`来取消设备上的IRP，也可以在内核中调用DDK的`IoCancelIrp()`函数来取消IRP。但是在DDK的`IoCancelIrp`函数内部会先获取一个取消自旋锁，在获取自旋锁后则调用取消回调例程。因此对于可能要取消的IRP一定要设置取消例程。调用`IoSetCancelRoutine()`函数可以为IRP设置取消例程，如果将第二个参数设置为`NULLL`也可以用于给IRP删除回调例程。如下为取消例程例子：

```
VOID CancelReadIrp(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
	KdPrint(("Enter CancelReadIrp\n"));

	pIrp->IoStatus.Status = STATUS_CANCELLED;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	IoReleaseCancelSpinLock(pIrp->CancelIrql);
	KdPrint(("Leave CancelReadIrp\n"));
}
```

即在取消例程中一方面要完成IRP，但是状态不是成功而是取消（STATUS_CANCELLED）；再者就要将取消自旋锁释放。

IRP同步中有一种实现串行化的方法，即`StartIO`，它可以将IRP请求进行序列化。`StartIO`例程运行在`DISPATCH_LEVEL`级别上，因此不能使用分页内存。除了DDK中提供的`StartIO`，还可以自定义`StartIO`。

这块内容用到时返回去看一下。


By Andy@2019-02-19 18:42:32
