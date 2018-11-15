
#第一个驱动程序#

这一篇文章列举三类现在用的驱动程序模型的基本程序，包括NT式驱动，`WDM`驱动程序和WDF驱动程序。列举代码，解析一下程序的基础知识，程序的编译以及程序的安装方法。

###NT式驱动程序###

应用程序开发时，都会有一个入口函数，比如控制台程序的`main()`函数。驱动程序也需要入口函数，它的入口函数为`DriverEntry`，从MSDN上可以看到该函数的声明如下。如同应用程序开发中需要`windows.h`一样，在驱动开发中也需要一个公共的头文件`ntddk.h`。

```
#include <ntddk.h>

NTSTATUS DriverEntry(
  _In_ PDRIVER_OBJECT  DriverObject,
  _In_ PUNICODE_STRING RegistryPath
)
{

	DriverObject->DriverUnload = Unload;

	return STATUS_SUCCESS;
}
```

对于最简单情况，驱动也需要指定卸载函数，否则在卸载驱动时会出现问题，如上指定卸载函数。卸载函数的原型如下代码所示。

```
DRIVER_UNLOAD Unload;

VOID Unload(
  _In_ struct _DRIVER_OBJECT *DriverObject
)
{
	return ;
}
```

如果要调试驱动，则需要在驱动入口处设置断点。使用内联汇编`__asm{int 3;}`。为了X86和X64统一这里可以使用`DebugBreak()`或`KdBreakPoint()`。

`DDKWizard`的下载地址[https://bitbucket.org/assarbad/ddkwizard/overview](https://bitbucket.org/assarbad/ddkwizard/overview)

下载`VisualDDK`地址[http://visualddk.sysprogs.org/](http://visualddk.sysprogs.org/)

###WDM驱动程序###


###WDF驱动程序###

待添加

**参考文章**

1. [http://www.coderjie.com/blog/91a5722cdd2d11e6841d00163e0c0e36](http://www.coderjie.com/blog/91a5722cdd2d11e6841d00163e0c0e36)


By Andy@2018-11-15 09:48:52
