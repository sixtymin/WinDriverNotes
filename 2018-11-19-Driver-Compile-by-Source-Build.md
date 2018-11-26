
#驱动编译文件编写#

编译驱动的方式有很多，在前面一篇文章中进行过简单总结，这里直接拿过来，如下列表所示。

1. 手动一行一行输入编译命令行和链接行。
2. 建立makefile文件，用nmake工具进行编译。
3. 建立makefile，sources，dirs文件用build工具编译。
4. 修改VC集成开发环境中Win32程序编译设置编译驱动。
5. 使用VirtualDDK，DDKWizard集成到低版本的VS中模板创建工程编译，同时包括EasySys创建编译工程。
6. 使用高版本的Visual Studio，比如VS2015等。

这里面使用`makefile`和`sources`文件进行编译可能是使用更多的时候，所以这里详细了解一下这些文件的编写方法。

###Sources等文件编写###

**sources**

build工具编译时依赖于sources文件中的设置，那些源文件需要编译，生成PE文件的类型（DLL，EXE还是SYS），包含目录路径，库目录路径等。

包含在sources文件中的内容其实是build工具中的变量（或者说宏定义），build通过解析这些宏定义，将宏定义内容转换为nmake编译需要的参数，传递给它。

对于参数的指定其实是修改build工具提供的变量，build会将这些变量反应给nmake工具，常用的几个build变量如下。

|   变量名称  |         含义     |         典型值        |
|------------|-----------------|----------------------|
|TARGETNAME  | 描述编译目标的名称 | 自定义                |
|TARGETTYPE  | 生成文件类别      | DRIVER/PROGRAM/DYNLINK|
|DDKROOT     | 设置DDK的根目录   | 一般不进行修改         |
|C_DEFINES   | C预编译定义参数，相当于#define| C_DEFINES=$(C_DEFINES) -DDBG形式|
|TARGETPATH  | 生成目标文件路径   | 自定义                |
|INCLUDES    | 设置包含目录的路径 | 自定义                |
|TARGETLIBS  | 设置目标代码所需库 | 自定义                |
|MSC_WARNING_LEVEL| 指明编译警告级别| 一般设置为 /W3      |
|SOURCES     | 设置待编译源码    | 使用换行符             |
|TARGETTEXT  | 定义生成文件后缀名 | .cpl等，可知定义为.dat |


其中有一个定义是必须要包含在内的：

`TARGETNAME`，这个宏指定了要构建的二进制文件的名字，名字中不能包含扩展名。

`TARGETTYPE`，这个宏用于指定要构建二进制的类型。比如要构建DLL，要设置参数`TARGETTYPE=DYNLINK`。其他的`PROGRAM`表示生成用户模式EXE程序，`PROGLIB`生成可执行程序，同时要为其他程序导出函数；`LIBRARY`表示生成静态链接库；`DRIVER_LIBRARY`表示生成内核态导入库；`DRIVER`表示生成内核模式驱动；`EXPORT_DRIVER`内核模式驱动，同时为其他驱动导出函数；`MINIPORT`表示生成内核模式驱动，但是不链接`ntoskrnl.lib`或`hal.lib`等；`GDI_DRIVER`表示生成内核模式图形驱动，链接`win32k.sys`模块，而非`ntoskrnl.dll`；`BOOTPGM`表示生成内核驱动，`HAL`表示是硬件抽象层程序。

`SOURCES`指定要编译文件的列表，文件之间用空格或制表符分割。这个宏可以深化，`I386_SOURCES`表示专用于X86的文件，`IA64_SOURCES`表示专用于IA64架构CPU源文件，`AMD64_SOURCES`表示用于X64的源文件，这样可以将所有文件写到一个Sources中。Build会优先处理这几个宏，再查看sources宏。在源码中想要加入汇编代码，比如X64汇编，可以使用`AMD64_SOURCES`宏来指定要编译进来的汇编文件。

`TARGETLIBS`用于指定其他想要链接进来的库，在这个宏中添加的库文件必须为绝对路径，可以使用BUILD的环境变量比如如下的形式。

```
TARGETLIBS=$(SDK_LIB_PATH)\kernel32.lib \
           $(SDK_LIB_PATH)\advapi32.lib \
           $(SDK_LIB_PATH)\user32.lib   \
           $(SDK_LIB_PATH)\spoolss.lib  \
           ..\mylib\obj\*\mylib.lib
```

如下，在WDK中给出了一个Sources文件的模板，使用WDK编译程序就可以直接修改这个模板来形成自己程序编译的Sources文件。

```
#
# The developer defines the TARGETNAME variable. It is the name of
# the target (component) that is being built by this makefile.
# It should not include any path or filename extension.
#
TARGETNAME=xxxxx
#
# The developer defines the TARGETPATH and TARGETTYPE variables.
# The first variable specifies where the target will be built. The second specifies
# the type of target (either PROGRAM, DYNLINK, LIBRARY, UMAPPL_NOLIB or
# BOOTPGM). Use UMAPPL_NOLIB when you are only building user-mode
# programs and do not need to build a library.
#
TARGETPATH=obj
# Select one of the following, and delete the others:
TARGETTYPE=PROGRAM
TARGETTYPE=DYNLINK
TARGETTYPE=LIBRARY
TARGETTYPE=UMAPPL_NOLIB
TARGETTYPE=BOOTPGM
TARGETTYPE=DRIVER
TARGETTYPE=DRIVER_LIBRARY
TARGETTYPE=EXPORT_DRIVER
TARGETTYPE=GDI_DRIVER
TARGETTYPE=MINIPORT
TARGETTYPE=NOTARGET
TARGETTYPE=PROGLIB#
# If your TARGETTYPE is DRIVER, you can optionally specify DRIVERTYPE.
# If you are building a WDM Driver, use DRIVERTYPE=WDM, if you are building
# a VxD use DRIVERTYPE=VXD. Otherwise, delete the following two lines.
#
DRIVERTYPE=WDM
DRIVERTYPE=VXD
#
# The TARGETLIBS macro specifies additional libraries to link against your target
# image. Each library path specification should contain an asterisk (*)
# where the machine-specific subdirectory name should go.
#
TARGETLIBS=
#
# The INCLUDES variable specifies any include paths that are specific to
# this source directory. Separate multiple paths with single
# semicolons. Relative path specifications are okay.
#
INCLUDES=..\inc
#
# The developer defines the SOURCES macro. It contains a list of all the
# source files for this component. Specify each source file on a separate
# line using the line-continuation character. This minimizes merge
# conflicts if two developers are adding source files to the same component.
#
SOURCES=source1.c \
        source2.c \
        source3.c \
        source4.c
        i386_SOURCES=i386\source1.asm
        IA64_SOURCES=ia64\source1.s
#
# Next, specify options for the compiler using C_DEFINES.
# All parameters specified here will be passed to both the C
# compiler and the resource compiler.
C_DEFINES=
#
# Next, specify one or more user-mode test programs and their type.
# Use UMTEST for optional test programs. Use UMAPPL for
# programs that are always built when the directory is built. See also
# UMTYPE, UMBASE, and UMLIBS. If you are building a driver, the next
# 5 lines should be deleted.
#
UMTYPE=nt
UMTEST=bunny*baz
UMAPPL=bunny*baz
UMBASE=0x1000000
UMLIBS=obj\*\bunny.lib
#
# Defining either (or both) the variables NTTARGETFILE0 and/or NTTARGETFILES
# causes makefile.def to include .\makefile.inc immediately after it
# specifies the top level targets (all, clean and loc) and their dependencies.
# The makefile.def file expands NTTARGETFILE0 as the first dependent for the
# "all" target and NTTARGETFILES as the last dependent for the "all" target.
# This is useful for specifying additional targets and dependencies that do not fit the
# general case covered by makefile.def.
#
# NTTARGETFILE0=
# NTTARGETFILES=
```

**makefile**

如果直接使用nmake命令进行编译，那么就需要编写详细的makefile文件了。这里使用build命令，其实makefile文件就很简单了，简单的原因是微软编写了一份makefile模板，它可以根据build传入的命令来按需使用其中的部分内容，这样就大大简化了makefile的编写。

大多数情况下在使用build命令编译驱动时只需要在makefile中简单写入如下的一段包含语句即可。

```
#
# DO NOT EDIT THIS FILE!!!  Edit .\sources. if you want to add a new source
# file to this component.  This file merely indirects to the real make file
# that is shared by all the driver components of the Windows NT DDK
#

!INCLUDE $(NTMAKEENV)\makefile.def
```

从一些工具生成的makefile文件中的注释也可以发现，警告使用者不要编写这个makefile文件，如果想要添加新的源文件编译sources即可。所以这个文件这里不再多说，只需`Copy&Paste`即可。

**dirs**

build工具可以递归地制定需要编译的文件和目录，dirs文件用来描述需要编译的子目录。如果在build命令执行的当前目录有`dirs`文件，build命令会分析此文件，依次进入这些子目录进行编译。这些子目录中可以包含其他的dirs文件或sources文件。

如下为WDK给出的样例源码目录中的dirs内容，其中每一个子目录都是一类驱动，其中还包含了dirs和sources文件。

```
DIRS= \
     1394 \
     audio \
     AVStream \
     biometrics \
     bth \
     filesys \
     general \
     hid \
     input \
     ir \
     mmedia \
     network \
     print \
     sd \
     SensorsAndLocation \
     serial \
     setup \
     sideshow \
     smartcrd \
     storage \
     swtuner \
     test \
     usb \
     video \
     videocap \
     wia \
     wmi \
     wpd
```

其实dirs文件仅仅用于告诉build命令，在当前目录中的一些子目录中还有文件需要编译，不要递归进入再执行build命令。

**makefile.inc**

`makefile.inc`文件是可选的，它是一个makefile文件，如果在编译目录存在这个文件，build会分析它，并且按照makefile的方式来执行其中的命令。

这里其实是放一些自定义make命令，由于makefile中内容不允许修改，所有想要自定义的内容都要被移到这个文件中。

###VC配置环境参数说明###

在使用build或新版VS驱动模板编译驱动时可能会遇到一些编译与链接的错误，了解一下VC配置驱动编译的各个选项有助于解决这些问题，同时也对修改其中的一些编译参数有启示作用。

如下为配置VC编译中需要设置的一些编译参数，以及它们代表的含义。

| VC编译参数 |             含义             |
|-----------|-----------------------------|
|/nologo    | 代表不显示编译器的版权信息      |
|/Gz        | 默认函数采用标准调用(__stdcall)|
|/W3        | 采用第三级警告模式            |
|/WX        | 将警告信息变成错误            |
|/Z7        | 用Z7模式产生调试信息，默认设置与/driver冲突|
|/Od        | 关闭编译器优化               |
|/Fo"output/"| 设置中间文件输出目录          |
|/Fd"output/"| 设置PDB文件放置的目录         |
|/FD        | 只生成文件依赖               |
|/c         | 只进行编译，不链接|

除上述内容外，还需要定义编译中使用的宏`/D WIN32=100 /D _X86_=1 /D WINVER=0x500 /D DBG=1`。

如下为配置VC编译中需要设置的一些链接参数，以及它们的含义。

|     VC链接参数        |             含义              |
|----------------------|------------------------------|
|ntoskrnl.lib          | NT驱动链接库，WDM需要`wdm.lib`  |
|/nologo               | 不显示编译器版权信息            |
|/base:"0x10000"       | 加载驱动时，设定加载虚拟内存位置  |
|/stack:0x400000,0x1000| 设定函数使用堆栈大小            |
|/entry:"DriverEntry"  | 驱动入口函数设定，它必须使用标准调用|
|/incremental:no       | 使用非递增式的链接               |
|/pdb:"Check/HelloDDK.pdb"| 设置PDB文件的文件名          |
|/debug                | 以debug方式链接                |
|/MACHINE:X86          | 产生代码是386兼容平台            |
|/nodefaultlib         | 不使用默认的库                  |
|/out:"Check/HelloDDK.sys"| 输出二进制模块名称            |
|/pdbtype:sept         | 设置PDB文件类型                 |
|/subsystem:native     | 设置子系统是内核系统             |
|/DRIVER               | 编译驱动                       |
|/SECTION:INIT,D       | 将INIT段设置为可卸载            |
|/IGNORE:4087          | 忽略4087号警告错误              |


By Andy@2018-11-19 09:37:52



