# Getting Started #

## Get

```
git clone git@github.com:hathach/tinyusb.git tinyusb
cd tinyusb
git submodule update --init
```

*examples* is the folder where all the application & project files are located. There are demos for both device and hosts. For each, there are different projects for each of supported RTOS. Click to have more information on how to [build](../examples/readme.md) and run [device](../examples/device/readme.md) demos.

## Add tinyusb to your project

It is relatively simple to incorporate tinyusb to your (existing) project

1. Copy or `git submodule` this repo into your project in a subfolder. Let's say it is *your_project/tinyusb*
2. Add all the .c in the src folder to your project settings (uvproj, ewp, makefile)
3. Add *your_project/tinysb* to your include path. Also make sure your current include path also contains the configuration file tusb_config.h. Or you could simply put the tusb_config.h into the tinyusb folder as well.
4. Make sure all required macros are all defined properly in tusb_config.h (configure file in demo application is sufficient, but you need to add a few more such as CFG_TUSB_MCU, CFG_TUSB_OS since they are passed by IDE/compiler to maintain a unique configure for all demo projects).
5. If you use the device stack, make sure you have created/modified usb descriptors for your own need. Ultimately you need to fill out required pointers in tusbd_descriptor_pointers for that stack to work.
6. Add tusb_init() call to your reset initialization code.
7. Implement all enabled classes's callbacks.
8. If you don't use any RTOSes at all, you need to continuously and/or periodically call tud_task()/tuh_task() function. Most of the callbacks and functionality are handled and invoke within the call of that task runner.

~~~{.c}
int main(void)
{
  your_init_code();
  tusb_init(); // initialize tinyusb stack

  while(1) // the mainloop
  {
    your_application_code();

    tud_task(); // tinyusb device task
    tuh_task(); // tinyusb host task
  }
}
~~~

[//]: # "\subpage md_boards_readme"
[//]: # "\subpage md_doxygen_started_demo"
[//]: # "\subpage md_tools_readme"
