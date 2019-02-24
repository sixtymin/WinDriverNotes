#调用其他驱动#

驱动中也会用到其他驱动的功能，这时候就需要在驱动中调用另外一个驱动程序。从驱动中调用其他驱动的方法有如下几种：

### 以文件句柄形式调用 ###

这种方法类似应用程序中调用驱动程序，这种方法很简单，不需要了解Windows底层知识。

这里直接调用`ZwCreateFile`用来打开设备，即可获取设备句柄。注意ZwCreateFile中的第三个参数`POBJECT_ATTRIBUTES ObjectAttribytes`用于指明要打开设备名，比如一直使用的`\\Device\\MyDDKDevice`。

与R3层的打开设备和操作设备类似，在驱动中使用`ZwCreateFile`等函数操作设备也有同步和异步区别，使用方法类似R3层，这里不再给出示例代码。

如果使用符号链接，也可以通过DDK函数获取符号链接对应的设备名。`ZwOpenSymbolicLinkObject`首先打开符号链接对象，`ZwQuerySymbolicLink()`函数可以查询符号链接对应的设备名，从而就可以使用上述的设备名打开设备并且进行调用了。

### 通过设备指针调用其他驱动 ###

`IoGetDeviceObjectPointer()`函数可以用于根据设备名称获取设备对象指针；或者也可以通过`ZwCreateFile`函数获取设备句柄，然后再通过`ObReferenceObjectByPointer()`函数通过句柄得到设备对象指针。

有了设备对象指针，就可以使用`IoCallDrver()`函数调用设备对象的派遣函数，即调用驱动。使用它进行调用还需要一个参数为IRP指针，所以需要进行手动构造。

```
// 创建同步IRP
IoBuildSynchronousFsdRequest();

// 创建异步IRP
IoBuildAsynchronousFsdRequest();

// 创建I/O控制的IRP
IoBuildDeviceIoControlRequest();

// IRP创建的通用函数，上述的函数都会最终调用到该函数。
IoAllocateIrp();
```

> ObReferenceObjectByName() 函数也可以用于获取设备指针；ObReferenceObjectByHandle()可以根据句柄获取对象指针；

驱动调用目前也没有用上，等用上了再回头复习一下调用方法，补充这块例子。


By Andy@2019-02-25 15:34:12
