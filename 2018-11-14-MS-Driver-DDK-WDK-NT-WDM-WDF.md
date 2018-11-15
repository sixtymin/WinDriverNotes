
#Windows驱动编程中的Mode概念#

在网上找Windows驱动编程相关的资料，以及在看介绍驱动编程的书的过程中，总会碰到很多基本概念。比如`DDK`，`WDK`，`NT式驱动`，`WDM`和`WDF`等。如果对于这些概念不了解，在一些书籍中说写一个`NT式驱动`时就会一脸蒙逼，不知所云；或者是在说要学习WDM时，同样的表情。这会最终导致还没有学习就被这些概念先打败了，所以在学习Windows驱动编写之前先学习一下这些概念。

先看一下`DDK`（Driver Developer Kit）和`WDK`（Windows Driver Kit）的区别，这个要说说驱动相关的一些历史。在`Win95/Win98/WinME`下驱动模型为`Vxd`，其编译二进制文件的后缀名为`.vxd`，在`Win2000/WinXP/Win2003`系统以后的Windows系统采用`WDM`驱动模型，要开发这两个模型的驱动，则需要开发包`DDK`(Driver Development Kit)，它们编译的驱动文件的扩展名为`.sys`。在Vista及以后版本中，驱动加入了WDF模型（Windows Driver Foudation），而它对应的开发包为`WDK`（Windows Driver Kit）。

其实`WDK`可以看做是`DDK`的升级版本，一般`WDK`是包含以前`DDK`开发包的相关功能。在XP下也可以用`WDK`开发驱动，`WDK`其实也能编译出`Win2000-Win2008`的各种驱动。

在不同的Windows版本上开发的驱动程序“模型”（模型这个词语应该来源于单词“Mode”。在Windows NT上，驱动程序被称为`Kernel Driver Mode`驱动程序。模型其实指一种驱动程序的结构和运作的规范）有过不同的名称。比如在`Windows 9x`上的驱动程序都叫做`Vxd`，而在Windows NT上的驱动程序被称为`KDM`驱动程序，`Windows 98～2000`这个时期出现的新模型叫做WDM。Windows的驱动模型概念，本来是就驱动程序的行为而言的。比如WDM驱动必须要满足提供几种被要求的特性（如电源管理、即插即用）才被称为WDM驱动。如果不提供这些功能，那么统一称为NT式驱动。同样的，WDF驱动也有它的一系列规范。

模型的发展并不是和操作系统版本的升级齐步走的，而是有一个逐渐替代的过程。比如`Windows 98`已经支持部分的`WDM`驱动程序，但是又支持`VXD`驱动。到了`Windows 2000`则VXD这种驱动程序完全被淘汰了。`KDM`则是`WDM`的前身。WDM是在KDM的基础上增加了一些新的特性，制定了一些新的规范而形成的。绝大部分函数调用都是通用的。

和`VXD`不同，从`KDM`到`WDM`再到`WDF`是一脉相承的，基本上`KDM`程序员在学习`WDM`时已经占尽了便宜。到`WDF`也不例外，`WDF`与其说是新的驱动开发模型，还不如说是在已有的内核API和数据结构的基础上，又封装出一套让使用者觉得更简单、更易用的以`Wdf`开头的一组`API`。

WDF又被分为了`KMDF`(Kernel Mode Driver Foundation)和`UMDF`(User Mode Driver Foundation)。用户模式可以确保驱动对系统的伤害比较小，如果出现致命性错误不会导致蓝屏或宕机。`UMDF`是如何实现的？驱动程序作为COM服务器运行，使用系统帐户，并且使用基于COM的通信框架，使得它看起来就像运行在内核模式。

**参考文章**

1. [https://yq.aliyun.com/articles/582162](https://yq.aliyun.com/articles/582162)
2. [http://driverentry.com.br/en/blog/?p=68](http://driverentry.com.br/en/blog/?p=68)
3. [http://blog.51cto.com/whatday/1382354](http://blog.51cto.com/whatday/1382354)
4. [https://blog.csdn.net/msk10k/article/details/51226666](https://blog.csdn.net/msk10k/article/details/51226666)
4. [http://blog.sina.com.cn/s/blog_4b9eab320101b6yn.html](http://blog.sina.com.cn/s/blog_4b9eab320101b6yn.html)

By Andy@2018-11-14 20:40:20

