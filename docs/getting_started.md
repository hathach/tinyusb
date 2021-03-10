# Getting Started #

## Add TinyUSB to your project

It is relatively simple to incorporate tinyusb to your (existing) project

- Copy or `git submodule` this repo into your project in a subfolder. Let's say it is *your_project/tinyusb*
- Add all the .c in the `tinyusb/src` folder to your project
- Add *your_project/tinyusb/src* to your include path. Also make sure your current include path also contains the configuration file tusb_config.h.
- Make sure all required macros are all defined properly in tusb_config.h (configure file in demo application is sufficient, but you need to add a few more such as CFG_TUSB_MCU, CFG_TUSB_OS since they are passed by IDE/compiler to maintain a unique configure for all boards).
- If you use the device stack, make sure you have created/modified usb descriptors for your own need. Ultimately you need to implement all **tud_descriptor_** callbacks for the stack to work.
- Add tusb_init() call to your reset initialization code.
- Call `tud_int_handler()` (device) and/or `tuh_int_handler()` (host) in your USB IRQ Handler
- Implement all enabled classes's callbacks.
- If you don't use any RTOSes at all, you need to continuously and/or periodically call tud_task()/tuh_task() function. All of the callbacks and functionality are handled and invoked within the call of that task runner.

~~~{.c}
int main(void)
{
  your_init_code();
  tusb_init(); // initialize tinyusb stack

  while(1) // the mainloop
  {
    your_application_code();

    tud_task(); // device task
    tuh_task(); // host task
  }
}
~~~

## Examples

For your convenience, TinyUSB contains a handful of examples for both host and device with/without RTOS to quickly test the functionality as well as demonstrate how API() should be used. Most examples will work on most of [the supported Boards](boards.md). Firstly we need to `git clone` if not already

```
$ git clone https://github.com/hathach/tinyusb tinyusb
$ cd tinyusb
```

Some TinyUSB examples also requires external submodule libraries in `/lib` such as FreeRTOS, Lightweight IP to build. Run following command to fetch them 

```
$ git submodule update --init lib
```

In addition, MCU driver submodule is also needed to provide low-level MCU peripheral's driver. Luckily, it will be fetched if needed when you run the `make` to build your board.

Note: some examples especially those that uses Vendor class (e.g webUSB) may requires udev permission on Linux (and/or macOS) to access usb device. It depends on your OS distro, typically copy `/examples/device/99-tinyusb.rules` file to /etc/udev/rules.d/ then run `sudo udevadm control --reload-rules && sudo udevadm trigger` is good enough.

### Build

To build example, first change directory to an example folder. 

```
$ cd examples/device/cdc_msc
```

Then compile with `make BOARD=[board_name] all`, for example

```
$ make BOARD=feather_nrf52840_express all
```

Note: `BOARD` can be found as directory name in `hw/bsp`, either in its family/boards or directly under bsp (no family).

#### Port Selection

If a board has several ports, one port is chosen by default in the individual board.mk file. Use option `PORT=x` To choose another port. For example to select the HS port of a STM32F746Disco board, use:

```
$ make BOARD=stm32f746disco PORT=1 all
```

#### Port Speed

A MCU can support multiple operational speed. By default, the example build system will use the fastest supported on the board. Use option `SPEED=full/high` e.g To force F723 operate at full instead of default high speed

```
$ make BOARD=stm32f746disco SPEED=full all
```

### Debug

To compile for debugging add `DEBUG=1`, for example

```
$ make BOARD=feather_nrf52840_express DEBUG=1 all
```

#### Log

Should you have an issue running example and/or submitting an bug report. You could enable TinyUSB built-in debug logging with optional `LOG=`. LOG=1 will only print out error message, LOG=2 print more information with on-going events. LOG=3 or higher is not used yet. 

```
$ make BOARD=feather_nrf52840_express LOG=2 all
```

#### Logger

By default log message is printed via on-board UART which is slow and take lots of CPU time comparing to USB speed. If your board support on-board/external debugger, it would be more efficient to use it for logging. There are 2 protocols: 

- `LOGGER=rtt`: use [Segger RTT protocol](https://www.segger.com/products/debug-probes/j-link/technology/about-real-time-transfer/)   
  - Cons: requires jlink as the debugger.
  - Pros: work with most if not all MCUs
  - Software viewer is JLink RTT Viewer/Client/Logger which is bundled with JLink driver package.
- `LOGGER=swo`: Use dedicated SWO pin of ARM Cortex SWD debug header.
  - Cons: only work with ARM Cortex MCUs minus M0
  - Pros: should be compatible with more debugger that support SWO.
  - Software viewer should be provided along with your debugger driver.

```
$ make BOARD=feather_nrf52840_express LOG=2 LOGGER=rtt all
$ make BOARD=feather_nrf52840_express LOG=2 LOGGER=swo all
```

### Flash

`flash` target will use the default on-board debugger (jlink/cmsisdap/stlink/dfu) to flash the binary, please install those support software in advance. Some board use bootloader/DFU via serial which is required to pass to make command

```
$ make BOARD=feather_nrf52840_express flash
$ make SERIAL=/dev/ttyACM0 BOARD=feather_nrf52840_express flash
```

Since jlink can be used with most of the boards, there is also `flash-jlink` target for your convenience.

```
$ make BOARD=feather_nrf52840_express flash-jlink
```

Some board use uf2 bootloader for drag & drop in to mass storage device, uf2 can be generated with `uf2` target

```
$ make BOARD=feather_nrf52840_express all uf2
```
