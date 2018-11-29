
#驱动编程中的Rtl与API#

在应用程序编写中需要使用SDK，其中提供了运行时和系统API等。同样，在驱动编程中也需要类似内容，这篇列举常用的运行时函数与API。

[TOC]

###内存管理###

Windows中内存管理使用了分段和分页相结合的方式，每个进程具有独立的虚拟地址空间，而内核地址空间则被所有进程共享。驱动程序运行时，不同的函数运行在不同的进程中，比如`DriverEntry`函数运行在System进程空间，而`IRP_MJ_READ/IRP_MJ_WRITE`等派遣函数则运行在执行读写的进程中。

驱动中的内存分配不同于应用层编程，不可以使用`malloc/new`等函数进行内存分配，除非对`new`进行重载。正规的内存分配要调用内核函数进行分配。在内核中，局部变量放在栈上，但是内核栈空间一般比较小，不适合递归调用或分配大型结构体类型的局部变量，对于大型的结构体需要在堆中进行分配。如下为堆中分配内存常用函数：

```
PVOID
NTAPI
ExAllocatePool(
    __in POOL_TYPE PoolType,    // 分配类型，可取值 NonPagedPool / PagedPool
    __in SIZE_T NumberOfBytes   // 分配大小，字节单位，最好4字节倍数
    );

PVOID
NTAPI
ExAllocatePoolWithTag(
    __in POOL_TYPE PoolType,
    __in SIZE_T NumberOfBytes,
    __in ULONG Tag              // 内存Tag
    );

PVOID
NTAPI
ExAllocatePoolWithQuota(
    __in POOL_TYPE PoolType,
    __in SIZE_T NumberOfBytes
    );

PVOID
NTAPI
ExAllocatePoolWithQuotaTag(
    __in POOL_TYPE PoolType,
    __in SIZE_T NumberOfBytes,
    __in ULONG Tag
    );
```

有内存分配函数，就有相应的内存释放，两类内存释放的函数如下：

```
VOID
NTAPI
ExFreePool(
    __in PVOID P	// 要释放的内存块指针
    );

VOID
NTAPI
ExFreePoolWithTag(
    __in PVOID P,
    __in ULONG Tag
    );
```

对于频繁堆内存的分配，上述的函数效率很低，并且很容易造成内存碎片。为了解决这个问题，DDK提供了Lookaside结构体。如果使用到了大量固定大小的小块内存，就可以使用该机制。

`Lookaside`结构类似一个内存池，使用时需要先初始化，当使用完毕要进行释放。其中常用函数如下所列：

```
// 初始化一个分页的Lookaside结构
VOID
ExInitializePagedLookasideList (
    __out PPAGED_LOOKASIDE_LIST Lookaside,
    __in_opt PALLOCATE_FUNCTION Allocate,
    __in_opt PFREE_FUNCTION Free,
    __in ULONG Flags,
    __in SIZE_T Size,
    __in ULONG Tag,
    __in USHORT Depth
    );

PVOID
ExAllocateFromPagedLookasideList (
    __inout PPAGED_LOOKASIDE_LIST Lookaside
    );

VOID
ExFreeToPagedLookasideList (
    __inout PPAGED_LOOKASIDE_LIST Lookaside,
    __in PVOID Entry
    );

VOID
ExDeletePagedLookasideList (
    __inout PPAGED_LOOKASIDE_LIST Lookaside
    );

// 初始化一个非分页内存的 Lookaside结构
VOID
ExInitializeNPagedLookasideList (
    __out PNPAGED_LOOKASIDE_LIST Lookaside,
    __in_opt PALLOCATE_FUNCTION Allocate,
    __in_opt PFREE_FUNCTION Free,
    __in ULONG Flags,
    __in SIZE_T Size,
    __in ULONG Tag,
    __in USHORT Depth
    );
```

**链表**

在操作系统中使用了大量的链表结构，查看`ETHREAD`,`EPROCESS`等结构就会发现其中有很多链表。DDK为了方便编程，提供了两种链表数据结构，单向链表和双向链表。

```
//
//  Doubly linked list structure.  Can be used as either a list head, or
//  as link words.
//

typedef struct _LIST_ENTRY {
   struct _LIST_ENTRY *Flink;
   struct _LIST_ENTRY *Blink;
} LIST_ENTRY, *PLIST_ENTRY, *RESTRICTED_POINTER PRLIST_ENTRY;

//
//  Singly linked list structure. Can be used as either a list head, or
//  as link words.
//

typedef struct _SINGLE_LIST_ENTRY {
    struct _SINGLE_LIST_ENTRY *Next;
} SINGLE_LIST_ENTRY, *PSINGLE_LIST_ENTRY;
```

在使用链表时，只需要将上述两个结构体当作数据类型在自己的结构体中声明成员即可，比如如下声明的一个结构体中。

```
typedef struct _MYDATA_STRUCT
{
    int x;
    int y;
    LIST_ENTRY lpList;
}MYDATA_STRUCT, *PMYDATA_STRUCT
```

DDK不单是定义了链表构建，还定义了操作函数，如下代码块所示。

```
// 初始化列表头
VOID
InitializeListHead(
    __out PLIST_ENTRY ListHead
    );

// 判断双向列表是否为空
BOOLEAN
IsListEmpty(
    __in const LIST_ENTRY * ListHead
    );

// 从链表头处移除一个节点
PLIST_ENTRY
RemoveHeadList(
    __inout PLIST_ENTRY ListHead
    );

// 从链表尾部移除一个节点
PLIST_ENTRY
RemoveTailList(
    __inout PLIST_ENTRY ListHead
    );

// 在链表尾部插入一个节点
VOID
InsertTailList(
    __inout PLIST_ENTRY ListHead,
    __inout PLIST_ENTRY Entry
    );

// 在链表头部插入一个节点
VOID
InsertHeadList(
    __inout PLIST_ENTRY ListHead,
    __inout PLIST_ENTRY Entry
    );

// 在链表尾部追加一个节点
VOID
AppendTailList(
    __inout PLIST_ENTRY ListHead,
    __inout PLIST_ENTRY ListToAppend
    );

// 单向链表的操作，压入与弹出操作
PSINGLE_LIST_ENTRY
PopEntryList(
    __inout PSINGLE_LIST_ENTRY ListHead
    );

VOID
PushEntryList(
    __inout PSINGLE_LIST_ENTRY ListHead,
    __inout PSINGLE_LIST_ENTRY Entry
    )
```

从上面结构体定义可以发现，`LIST_ENTRY`成员并不一定在结构体头部，DDK提供了一个宏，可以从结构体中列表成员获取结构体起始地址。

```
#define CONTAINING_RECORD(address, type, field) ((type *)( \
                                                 (PCHAR)(address) - \
                                                 (ULONG_PTR)(&((type *)0)->field)))
```

**内存运行时函数**

关于内存的常用内存时函数如下所示：

```
// 内存拷贝（非重叠）
VOID
RtlCopyMemory(
	IN VOID *Destination,
	IN CONST VOID *Source,
	IN SIZE_T Length);

// 内存拷贝（内存可重叠）
VOID
RtlMoveMemory(
	IN VOID *Destination,
	IN CONST VOID *Source,
	IN SIZE_T Length);

// 内存填充
VOID
RtlFillMemory(
	IN VOID *Destination,
	IN SIZE_T Length,
	IN UCHAR Fill);

// 内存清零
VOID
RtlZeroMemory(
	IN VOID *Destination,
	IN SIZE_T Length)

// 判断内存块相同
VOID
RtlEqualMemory(
	CONST VOID *Source1,
	CONST VOID *Source2,
	SIZE_T Length);

// 内存对比
SIZE_T
NTAPI
RtlCompareMemory(
    _In_ const VOID * Source1,
    _In_ const VOID * Source2,
    _In_ SIZE_T Length
    );
```

内存读写常会涉及到这块内存到底可不可以读写，如果写是否内存块为只读，如果读是否内存块是未分配地址空间？内核中有两个函数用于查看内存是否可读或可写！

```
// 判断内存Address指向内存是否Length长度可读
VOID
NTAPI
ProbeForRead (
    __in PVOID Address,
    __in SIZE_T Length,
    __in ULONG Alignment
    );

// 判断内存Address指向内存是否Length长度可写
VOID
NTAPI
ProbeForWrite (
    __in PVOID Address,
    __in SIZE_T Length,
    __in ULONG Alignment
    );
```

这两个函数在判断内存可读写时，如果不是可读写的情况，它们会引发异常，这就需要使用系统提供的结构化异常进行处理，否则就导致异常处理不了出现蓝屏了。如下异常处理结构：

```
__try
{

}
__except(EXCEPTION_EXECUTE_HANDLER)
{
}

// 或
__try
{

}
__finally
{
	KdPrint(("Enter finally block\n"));
}
```

###内核函数###

这一节列举常用的内核函数，包括字符串处理，注册表操作，文件操作等。

**字符串处理**

在内核中并不直接使用字符串，而是使用包含字符串的`ANSI_STRING`或`UNICODE_STRING`结构来替代单纯字符串指针，它们的定义如下所示：

```
typedef struct _STRING {
    USHORT Length;			// 字符串长度，不带结束字符'\0'
    USHORT MaximumLength;	// 该结构可以容纳最大字符串长度
    PCHAR Buffer;			// 放置字符串的缓存
} STRING;
typedef STRING *PSTRING;

typedef STRING ANSI_STRING;
typedef PSTRING PANSI_STRING;
typedef PSTRING PCANSI_STRING;

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING;
typedef UNICODE_STRING *PUNICODE_STRING;
typedef const UNICODE_STRING *PCUNICODE_STRING;
```

这些结构在内核中有相应的函数可以操作它们，如下代码块：

```
// 初始化结构体
VOID
NTAPI
RtlInitAnsiString(
    __out PANSI_STRING DestinationString,
    __in_z_opt __drv_aliasesMem PCSZ SourceString
    );

VOID
NTAPI
RtlInitUnicodeString(
    __out PUNICODE_STRING DestinationString,
    __in_z_opt __drv_aliasesMem PCWSTR SourceString
    );
// 下面函数都有一一对应的，这里只列举UNICODE_STRING相关函数

// 拷贝字符串
VOID
NTAPI
RtlCopyUnicodeString(
    __inout PUNICODE_STRING DestinationString,
    __in_opt PCUNICODE_STRING SourceString
    );

// 字符串比较
LONG
NTAPI
RtlCompareUnicodeString(
    __in PCUNICODE_STRING String1,
    __in PCUNICODE_STRING String2,
    __in BOOLEAN CaseInSensitive		// 比较字符串是否对大小写敏感
    );

BOOLEAN
NTAPI
RtlEqualUnicodeString(
    __in PCUNICODE_STRING String1,
    __in PCUNICODE_STRING String2,
    __in BOOLEAN CaseInSensitive
    );

// 字符串全都转换为大写字符
WCHAR
NTAPI
RtlUpcaseUnicodeChar(
    __in WCHAR SourceCharacter
    );

NTSTATUS
NTAPI
RtlUpcaseUnicodeString(
    __out PUNICODE_STRING DestinationString,
    __in PCUNICODE_STRING SourceString,
    __in BOOLEAN AllocateDestinationString // 是否为结果字符串分配内存
    );

// 在一个字符串后追加字符串
NTSTATUS
NTAPI
RtlAppendUnicodeStringToString (
    __inout PUNICODE_STRING Destination,
    __in PCUNICODE_STRING Source
    );
NTSTATUS
NTAPI
RtlAppendUnicodeToString (
    __inout PUNICODE_STRING Destination,
    __in_z_opt PCWSTR Source
    );

// ANSI_STRING转UNICODE_STRING
NTSTATUS
NTAPI
RtlAnsiStringToUnicodeString(
    __inout PUNICODE_STRING DestinationString,
    __in PCANSI_STRING SourceString,
    __in BOOLEAN AllocateDestinationString
    );

// 将字符串转为整数
NTSTATUS
NTAPI
RtlUnicodeStringToInteger (
    __in PCUNICODE_STRING String,
    __in_opt ULONG Base,			// 数基，10进制，二进制
    __out PULONG Value
    );
// 将整数转化为字符串
NTSTATUS
NTAPI
RtlIntegerToUnicodeString (
    __in ULONG Value,
    __in_opt ULONG Base,
    __inout PUNICODE_STRING String
    );

// 释放UNICODE_STRING，这是针对Buffer成员动态分配的情况
VOID
NTAPI
RtlFreeUnicodeString(
    __inout PUNICODE_STRING UnicodeString
    );
```

**文件操作**

内核中操作文件与Win32 API不同，DDK提供的是另外一套操作函数。

```
NTSTATUS
NTAPI
ZwCreateFile(
    __out PHANDLE FileHandle,					// 返回文件句柄
    __in ACCESS_MASK DesiredAccess,
    __in POBJECT_ATTRIBUTES ObjectAttributes,	// 包含文件路径
    __out PIO_STATUS_BLOCK IoStatusBlock,		// 返回状态信息块，必填
    __in_opt PLARGE_INTEGER AllocationSize,
    __in ULONG FileAttributes,
    __in ULONG ShareAccess,
    __in ULONG CreateDisposition,
    __in ULONG CreateOptions,
    __in_bcount_opt(EaLength) PVOID EaBuffer,
    __in ULONG EaLength
    );

// 路径需要使用该函数进行初始化，它其实是一个宏。
VOID
InitializeObjectAttributes(
	OUT POBJECT_ATTRIBUTES InitializedAttributes,
	IN PUNICODE_STRING ObjectName,
	IN ULONG Attribuites,
	IN HANDLE RootDirectory,
	IN PSECURITY_DESCRIPTOR SecurityDescriptor);
```

在内核中文件路径需要使用设备符号链接，即对于`C:\file.log`应该写成`\??\C:\file.log`，由内核将符号链接转为真正的的设备名称。

其他的文件操作的函数如下：

```
// 打开文件，较ZwCreateFile简单一些，针对已存文件
NTSTATUS
NTAPI
ZwOpenFile(
    __out PHANDLE FileHandle,
    __in ACCESS_MASK DesiredAccess,
    __in POBJECT_ATTRIBUTES ObjectAttributes,
    __out PIO_STATUS_BLOCK IoStatusBlock,
    __in ULONG ShareAccess,
    __in ULONG OpenOptions
    );

// 查看文件的信息，依据FileInformationClass有不同的传入FileInformation值
NTSTATUS
NTAPI
ZwQueryInformationFile(
    __in HANDLE FileHandle,
    __out PIO_STATUS_BLOCK IoStatusBlock,
    __out_bcount(Length) PVOID FileInformation,
    __in ULONG Length,
    __in FILE_INFORMATION_CLASS FileInformationClass
    );

// 该枚举类型就比较多了，不一一列举，使用时查看WDK帮助文档
typedef enum _FILE_INFORMATION_CLASS {
    FileDirectoryInformation         = 1,
    FileFullDirectoryInformation,   // 2
    FileBothDirectoryInformation,   // 3
    FileBasicInformation,           // 4
    FileStandardInformation,        // 5
    ......
}FILE_INFORMATION_CLASS;

// 设置文件属性，类上
NTSTATUS
NTAPI
ZwSetInformationFile(
    __in HANDLE FileHandle,
    __out PIO_STATUS_BLOCK IoStatusBlock,
    __in_bcount(Length) PVOID FileInformation,
    __in ULONG Length,
    __in FILE_INFORMATION_CLASS FileInformationClass
    );

NTSTATUS
NTAPI
ZwReadFile(
    __in HANDLE FileHandle,
    __in_opt HANDLE Event,
    __in_opt PIO_APC_ROUTINE ApcRoutine,
    __in_opt PVOID ApcContext,
    __out PIO_STATUS_BLOCK IoStatusBlock,
    __out_bcount(Length) PVOID Buffer,
    __in ULONG Length,
    __in_opt PLARGE_INTEGER ByteOffset,
    __in_opt PULONG Key
    );

NTSTATUS
NTAPI
ZwWriteFile(
    __in HANDLE FileHandle,
    __in_opt HANDLE Event,
    __in_opt PIO_APC_ROUTINE ApcRoutine,
    __in_opt PVOID ApcContext,
    __out PIO_STATUS_BLOCK IoStatusBlock,
    __in_bcount(Length) PVOID Buffer,
    __in ULONG Length,
    __in_opt PLARGE_INTEGER ByteOffset,
    __in_opt PULONG Key
    );

// 关闭打开的文件句柄
NTSTATUS
NTAPI
ZwClose(
    __in HANDLE Handle
    );
```

**内核注册表操作**

其实注册表操作和文件类似，如下列举操作函数：

```
// 打开注册表键 Key
NTSTATUS
NTAPI
ZwCreateKey(
    __out PHANDLE KeyHandle,
    __in ACCESS_MASK DesiredAccess,
    __in POBJECT_ATTRIBUTES ObjectAttributes,
    __reserved ULONG TitleIndex,
    __in_opt PUNICODE_STRING Class,
    __in ULONG CreateOptions,
    __out_opt PULONG Disposition
    );

// 打开注册表
NTSTATUS
NTAPI
ZwOpenKey(
    __out PHANDLE KeyHandle,
    __in ACCESS_MASK DesiredAccess,
    __in POBJECT_ATTRIBUTES ObjectAttributes
    );

NTSTATUS
NTAPI
ZwOpenKeyEx(
    __out PHANDLE KeyHandle,
    __in ACCESS_MASK DesiredAccess,
    __in POBJECT_ATTRIBUTES ObjectAttributes,
    __in ULONG OpenOptions
    );

// 删除子项
NTSTATUS
NTAPI
ZwDeleteKey(
    __in HANDLE KeyHandle
    );

NTSTATUS
NTAPI
ZwDeleteValueKey(
    __in HANDLE KeyHandle,
    __in PUNICODE_STRING ValueName
    );

// 查询一个键的子健的数量和大小
NTSTATUS
NTAPI
ZwQueryKey(
    __in HANDLE KeyHandle,
    __in KEY_INFORMATION_CLASS KeyInformationClass,
    __out_bcount_opt(Length) PVOID KeyInformation,
    __in ULONG Length,
    __out PULONG ResultLength
    );

// 枚举Key，遍历key下的内容，它常与 ZwQueryKey结合使用
NTSTATUS
NTAPI
ZwEnumerateKey(
    __in HANDLE KeyHandle,
    __in ULONG Index,
    __in KEY_INFORMATION_CLASS KeyInformationClass,
    __out_bcount_opt(Length) PVOID KeyInformation,
    __in ULONG Length,
    __out PULONG ResultLength
    );

// 枚举子键
NTSTATUS
NTAPI
ZwEnumerateValueKey(
    __in HANDLE KeyHandle,
    __in ULONG Index,
    __in KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
    __out_bcount_opt(Length) PVOID KeyValueInformation,
    __in ULONG Length,
    __out PULONG ResultLength
    );

// 查询注册表某个键的值
NTSTATUS
NTAPI
ZwQueryValueKey(
    __in HANDLE KeyHandle,
    __in PUNICODE_STRING ValueName,
    __in KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
    __out_bcount_opt(Length) PVOID KeyValueInformation,
    __in ULONG Length,
    __out PULONG ResultLength
    );



NTSTATUS
NTAPI
ZwSetInformationKey(
    __in HANDLE KeyHandle,
    __in KEY_SET_INFORMATION_CLASS KeySetInformationClass,
    __in_bcount(KeySetInformationLength) PVOID KeySetInformation,
    __in ULONG KeySetInformationLength
    );

// 添加和修改注册表键值
NTSTATUS
NTAPI
ZwSetValueKey(
    __in HANDLE KeyHandle,
    __in PUNICODE_STRING ValueName,
    __in_opt ULONG TitleIndex,
    __in ULONG Type,
    __in_bcount_opt(DataSize) PVOID Data,
    __in ULONG DataSize
    );
```

注册表操作也是获取的HANDLE，所以它的关闭类似文件操作，调用`ZwClose`即可。

其实DDK提供了一系列的Rtl函数来操作注册表，其实就是封装了前面说的一些函数。

```
RtlCreateRegistryKey		// 创建注册表
RtlCheckRegistryKey			// 查看某注册表项是否存在
RtlWriteRegistryValue		// 写注册表
RtlDeleteRegistryValue		// 删除注册表子键
RtlQueryRegistryValues		// 查询子键键值
```

By Andy@2018-11-29 07:22:32




