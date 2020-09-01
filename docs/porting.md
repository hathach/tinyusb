# Porting

TinyUSB is designed to be a universal USB protocol stack for microcontrollers. It
handles most of the high level USB protocol and relies on the microcontroller's USB peripheral for
data transactions on different endpoints. Porting is the process of adding low-level support for
the rest of the common stack. Once the low-level is implemented, it is very easy to add USB support
for the microcontroller to other projects, especially those already using TinyUSB such as CircuitPython.

Below are instructions on how to get the cdc_msc device example running on a new microcontroller. Doing so includes adding the common code necessary for other uses while minimizing other extra code. Whenever you see a phrase or word in <> it should be replaced.

## Register defs

The first step to adding support is including the register definitions and startup code for the
microcontroller in TinyUSB. We write the TinyUSB implementation against these structs instead of higher level functions to keep the code small and to prevent function name collisions in linking of larger projects. For ARM microcontrollers this is the CMSIS definitions. They should be
placed in the `hw/mcu/<vendor>/<chip_family>` directory.

Once this is done, create a directory in `hw/bsp/<your board name>` for the specific board you are using to test the code.  (Duplicating an existing board's directory is the best way to get started.) The board should be a readily available development board so that others can also test.

## Build
Now that those directories are in place, we can start our iteration process to get the example building successfully. To build, run from the root of TinyUSB:

`make -C examples/device/cdc_msc BOARD=<board>`

Unless, you've read ahead, this will fail miserably. Now, lets get it to fail less by updating the files in the board directory. The code in the board's directory is responsible for setting up the microcontroller's clocks and pins so that USB works. TinyUSB itself only operates on the USB peripheral. The board directory also includes information what files are needed to build the example.

One of the first things to change is the `-DCFG_TUSB_MCU` cflag in the `board.mk` file. This is used to tell TinyUSB what platform is being built. So, add an entry to `src/tusb_option.h` and update the CFLAG to match.

Update `board.mk`'s VENDOR and CHIP_FAMILY values when creating the directory for the struct files. Duplicate one of the other sources from `src/portable` into `src/portable/<vendor>/<chip_family>` and delete all of the implementation internals. We'll cover what everything there does later. For now, get it compiling.

## Implementation
At this point you should get an error due to an implementation issue and hopefully the build is setup for the new MCU. You will still need to modify the `board.mk` to include specific CFLAGS, the linker script, linker flags, source files, include directories. All file paths are relative to the top of the TinyUSB repo.

### Board Support (BSP)
The board support code is only used for self-contained examples and testing. It is not used when TinyUSB is part of a larger project. Its responsible for getting the MCU started and the USB peripheral clocked. It also optionally provides LED definitions that are used to blink an LED to show that the code is running.

It is located in `hw/bsp/<board name>/board_<board name>.c`.

#### board_init
`board_init` is responsible for starting the MCU, setting up the USB clock and USB pins. It is also responsible for initializing LED pins.

One useful clock debugging technique is to set up a PWM output at a known value such as 500hz based on the USB clock so that you can verify it is correct with a logic probe or oscilloscope.

Setup your USB in a crystal-less mode when available. That makes the code easier to port across boards.

#### board_led_write
Feel free to skip this until you want to verify your demo code is running. To implement, set the pin corresponding to the led to output a value that lights the LED when `state` is true.

### OS Abstraction Layer (OSAL)

The OS Abstraction Layer is responsible for providing basic data structures for TinyUSB that may allow for concurrency when used with an RTOS. Without an RTOS it simply handles concurrency issues between the main code and interrupts.

The code is almost entirely agnostic of MCU and lives in `src/osal`.

### Device API

After the USB device is setup, the USB device code works by processing events on the main thread (by calling `tud_task`). These events are queued by the USB interrupt handler. So, there are three parts to the device low-level API: device setup, endpoint setup and interrupt processing.

All of the code for the low-level device API is in `src/portable/<vendor>/<chip family>/dcd_<chip family>.c`.

#### Device Setup

##### dcd_init

Initializes the USB peripheral for device mode and enables it.
This function should enable internal D+/D- pull-up for enumeration.

##### dcd_int_enable / dcd_int_disable

Enables or disables the USB device interrupt(s). May be used to prevent concurrency issues when mutating data structures shared between main code and the interrupt handler.

##### dcd_int_handler

Processes all the hardware generated events e.g Bus reset, new data packet from host etc ... It will be called by application in the MCU USB interrupt handler.

##### dcd_set_address

Called when the device is given a new bus address.

If your peripheral automatically changes address during enumeration (like the nrf52) you may leave this empty and also no queue an event for the corresponding SETUP packet.

##### dcd_remote_wakeup

Called to remote wake up host when suspended (e.g hid keyboard)

##### dcd_connect / dcd_disconnect

Connect or disconnect the data-line pull-up resistor. Define only if MCU has an internal pull-up. (BSP may define for MCU without internal pull-up.)

#### Special events

You must let TinyUSB know when certain events occur so that it can continue its work. There are a few methods you can call to queue events for TinyUSB to process.

##### dcd_event_bus_signal

There are a number of events that your peripheral may communicate about the state of the bus. Here is an overview of what they are. Events in **BOLD** must be provided for TinyUSB to work.

* **DCD_EVENT_RESET** - Triggered when the host resets the bus causing the peripheral to reset. Do any other internal reset you need from the interrupt handler such as resetting the control endpoint.
* DCD_EVENT_SOF - Signals the start of a new USB frame.

Calls to this look like:

    dcd_event_bus_signal(0, DCD_EVENT_BUS_RESET, true);

The first `0` is the USB peripheral number. Statically saying 0 is common for single USB device MCUs.

The `true` indicates the call is from an interrupt handler and will always be the case when porting in this way.

##### dcd_setup_received

SETUP packets are a special type of transaction that can occur at any time on the control endpoint, numbered `0`. Since they are unique, most peripherals have special handling for them. Their data is always 8 bytes in length as well.

Calls to this look like:

    dcd_event_setup_received(0, setup, true);

As before with `dcd_event_bus_signal` the first argument is the USB peripheral number and the third is true to signal its being called from an interrupt handler. The middle argument is byte array of length 8 with the contents of the SETUP packet. It can be stack allocated because it is copied into the queue.

#### Endpoints

Endpoints are the core of the USB data transfer process. They come in a few forms such as control, isochronous, bulk, and interrupt. We won't cover the details here except with some caveats in open below. In general, data is transferred by setting up a buffer of a given length to be transferred on a given endpoint address and then waiting for an interrupt to signal that the transfer is finished. Further details below.

Endpoints within USB have an address which encodes both the number and direction of an endpoint. TinyUSB provides `tu_edpt_number` and `tu_edpt_dir` to unpack this data from the address. Here is a snippet that does it.

    uint8_t epnum = tu_edpt_number(ep_addr);
    uint8_t dir   = tu_edpt_dir(ep_addr);

##### dcd_edpt_open

Opening an endpoint is done for all non-control endpoints once the host picks a configuration that the device should use. At this point, the endpoint should be enabled in the peripheral and configured to match the endpoint descriptor. Pay special attention to the direction of the endpoint you can get from the helper methods above. It will likely change what registers you are setting.

Also make sure to enable endpoint specific interrupts.

##### dcd_edpt_close

Close an endpoint. his function is used for implementing alternate settings.

After calling this, the device should not respond to any packets directed towards this endpoint. When called, this function must abort any transfers in progress through this endpoint, before returning.

Implementation is optional. Must be called from the USB task. Interrupts could be disabled or enabled during the call.

##### dcd_edpt_xfer

`dcd_edpt_xfer` is responsible for configuring the peripheral to send or receive data from the host. "xfer" is short for "transfer". **This is one of the core methods you must implement for TinyUSB to work (one other is the interrupt handler).**  Data from the host is the OUT direction and data to the host is IN. It  is used for all endpoints including the control endpoint 0. Make sure to handle the zero-length packet STATUS packet on endpoint 0 correctly. It may be a special transaction to the peripheral.

Besides that, all other transactions are relatively straight-forward. The endpoint address provides the endpoint
number and direction which usually determines where to write the buffer info. The buffer and its length are usually
written to a specific location in memory and the peripheral is told the data is valid. (Maybe by writing a 1 to a
register or setting a counter register to 0 for OUT or length for IN.)

The transmit buffer alignment is determined by `CFG_TUSB_MEM_ALIGN`.

One potential pitfall is that the buffer may be longer than the maximum endpoint size of one USB
packet. Some peripherals can handle transmitting multiple USB packets for a provided buffer (like the SAMD21).
Others (like the nRF52) may need each USB packet queued individually. To make this work you'll need to track
some state for yourself and queue up an intermediate USB packet from the interrupt handler.

Once the transaction is going, the interrupt handler will notify TinyUSB of transfer completion.
During transmission, the IN data buffer is guarenteed to remain unchanged in memory until the `dcd_xfer_complete` function is called.

The dcd_edpt_xfer function must never add zero-length-packets (ZLP) on its own to a transfer. If a ZLP is required,
then it must be explicitly sent by the stack calling dcd_edpt_xfer(), by calling dcd_edpt_xfer() a second time with len=0.
For control transfers, this is automatically done in `usbd_control.c`.

At the moment, only a single buffer can be transmitted at once. There is no provision for double-buffering. new dcd_edpt_xfer() will not
be called again on the same endpoint address until the driver calls dcd_xfer_complete() (except in cases of USB resets).

##### dcd_xfer_complete

Once a transfer completes you must call dcd_xfer_complete from the USB interrupt handler to let TinyUSB know that a transaction has completed. Here is a sample call:

    dcd_event_xfer_complete(0, ep_addr, xfer->actual_len, XFER_RESULT_SUCCESS, true);

The arguments are:
* the USB peripheral number
* the endpoint address
* the actual length of the transfer. (OUT transfers may be smaller than the buffer given in `dcd_edpt_xfer`)
* the result of the transfer. Failure isn't handled yet.
* `true` to note the call is from an interrupt handler.

##### dcd_edpt_stall / dcd_edpt_clear_stall

Stalling is one way an endpoint can indicate failure such as when an unsupported command is transmitted. The pair of `dcd_edpt_stall`, `dcd_edpt_clear_stall` help manage the stall state of all endpoints.

## Woohoo!

At this point you should have everything working! ;-) Of course, you may not write perfect code. Here are some tips and tricks for debugging.

Use [WireShark](https://www.wireshark.org/) or [a Beagle](https://www.totalphase.com/protocols/usb/) to sniff the USB traffic. When things aren't working its likely very early in the USB enumeration process. Figuring out where can help clue in where the issue is. For example:
* If the host sends a SETUP packet and its not ACKed then your USB peripheral probably isn't started correctly.
* If the peripheral is started correctly but it still didn't work, then verify your usb clock is correct. (You did output a PWM based on it right? ;-) )
* If the SETUP packet is ACKed but nothing is sent back then you interrupt handler isn't queueing the setup packet correctly. (Also, if you are using your own code instead of an example `tud_task` may not be called.) If thats OK, the `dcd_xfer_complete` may not be setting up the next transaction correctly.
