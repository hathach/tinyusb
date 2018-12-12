# Getting Started #

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**

- [Download](#download)
- [Add tinyusb to your project](#add-tinyusb-to-your-project)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

## Download

tinyusb uses github as online repository https://github.com/hathach/tinyusb since it is the best place for open source project.

If you are using Linux, you already know how to what to do. But If Windows is your OS, I would suggest to install [git](http://git-scm.com/) and front-end gui such as [tortoisegit](http://code.google.com/p/tortoisegit) to begin with.

After downloading/cloning, the code base is composed of

Folder  | Description
-----   | -------------
doxygen | Documentation
examples| Folder where test examples are kept with Makefile and Segger Embedded build support
hw/bsp  | Source files of supported boards
hw/mcu  | Low level mcu core & peripheral drivers (e.g CMSIS )
lib     | Source files from 3rd party such as freeRTOS, fatfs etc ...
src     | All sources files for tinyusb stack itself.
tests   | Unit tests for the stack
tools   | Files used internally

*examples* is the folder where all the application & project files are located. There are demos for both device and hosts. For each, there are different projects for each of supported RTOS. Click to have more information on how to [build](../examples/readme.md) and run [device](../examples/device/readme.md) demos.

## Add tinyusb to your project

It is relatively simple to incorporate tinyusb to your (existing) project

1. Copy or `git submodule` this repo into your project in a subfolder. Let's say it is *your_project/tinyusb*
2. Add all the .c in the src folder to your project settings (uvproj, ewp, makefile)
3. Add *your_project/tinysb* to your include path. Also make sure your current include path also contains the configuration file tusb_config.h. Or you could simply put the tusb_config.h into the tinyusb folder as well.
4. Make sure all required macros are all defined properly in tusb_config.h (configure file in demo application is sufficient, but you need to add a few more such as CFG_TUSB_MCU, CFG_TUSB_OS, CFG_TUD_TASK_PRIO since they are passed by IDE/compiler to maintain a unique configure for all demo projects).
5. If you use the device stack, make sure you have created/modified usb descriptors for your own need. Ultimately you need to fill out required pointers in tusbd_descriptor_pointers for that stack to work.
6. Add tusb_init() call to your reset initialization code.
7. Implement all enabled classes's callbacks.
8. If you don't use any RTOSes at all, you need to continuously and/or periodically call tusb_task() function. Most of the callbacks and functionality are handled and invoke within the call of that task runner.

~~~{.c}
int main(void)
{
  your_init_code();
  tusb_init(); // initialize tinyusb stack

  while(1) // the mainloop
  {
    your_application_code();

    tusb_task(); // handle tinyusb event, task etc ...
  }
}
~~~

[//]: # (\subpage md_boards_readme)
[//]: # (\subpage md_doxygen_started_demo)
[//]: # (\subpage md_tools_readme)
