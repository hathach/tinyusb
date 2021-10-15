TinyUSB for RT-Thread Port
==========================

中文 \| `English <./readme.rst>`__

TinyUSB 是一个用于嵌入式设备的跨平台 USB 协议栈。

1、 打开 TinyUSB
----------------

RT-Thread 包管理器中的路径如下：

.. code:: text

    -> RT-Thread online packages
        -> system packages
            --- TinyUSB (offical): an open source cross-platform USB stack for embedded system
                (2048) TinyUSB thread stack size
                [*]   Using USB device  ---->
                    [*]   Using Communication Device Class (CDC)
                    [*]   Using Mass Storage Class (MSC)
                    ()      The name of the device used by MSC
                      Version (latest)  --->

功能配置说明如下：

-  TinyUSB 线程栈大小
-  是否使用 USB 设备

   -  是否使用 CDC
   -  是否使用 MSC
   -  用于 MSC 读写的块设备名字

然后让 RT-Thread 的包管理器自动更新，或者使用 ``pkgs --update``
命令更新包到 BSP 中。

2、支持情况
-----------

2.1、MCU
~~~~~~~~

目前仅支持 STM32 系列 MCU。

2.2、设备类
~~~~~~~~~~~

-  通信设备类（CDC）
-  大容量存储设备（MSC）

3、Feedback
-----------

issue: `tfx2001/tinyusb <https://github.com/tfx2001/tinyusb/issues>`__
