# HID Boot Keyboard and Mouse

A composite USB HID device that exposes a boot-protocol keyboard and a boot-protocol mouse as two separate interfaces.

## What it does

- Presents two HID interfaces: a boot keyboard (interface 0) and a boot mouse (interface 1).
- Polls the board button every 10 ms. While the button is held, the keyboard sends the Right Arrow keycode and the mouse moves diagonally (+5, +5); releasing the button sends an empty keyboard report.
- Uses the HID boot protocol, so the device works even before an OS HID driver loads (e.g. in a PC BIOS/UEFI setup).
- If the device is suspended, pressing the button issues a USB remote wakeup.
- The LED blinks to indicate USB state (250 ms not mounted, 1000 ms mounted, 2500 ms suspended). When the host turns on Caps Lock, the LED is driven solid on via the keyboard's OUTPUT report.

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0 | HID (boot keyboard) |
| 1 | HID (boot mouse) |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_HID            2 // boot keyboard + boot mouse
#define CFG_TUD_HID_EP_BUFSIZE 8
```

## Building

CMake:

```bash
mkdir build && cd build
cmake -DBOARD=raspberry_pi_pico ..
cmake --build .
```

Make:

```bash
make BOARD=raspberry_pi_pico all
```

## Try it

After flashing, the board enumerates as a keyboard and a mouse. Press and hold the button: the host receives repeated Right Arrow key presses and the pointer drifts toward the bottom-right. Because it uses the boot protocol, the keyboard also works in a BIOS/UEFI menu.
