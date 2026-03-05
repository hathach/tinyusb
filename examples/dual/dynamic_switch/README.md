# Dynamic Switch Example

This example demonstrates TinyUSB's dual-role capability by allowing runtime switching between USB device and host modes.

## Features

- **Button-triggered mode switching**: Press the board button to switch between device and host modes
- **Device Mode**: Acts as a USB CDC (Virtual Serial Port) that echoes all received data
- **Host Mode**: Enumerates connected USB devices and prints device information
- **Dynamic switching**: Deinitializes the current stack and reinitializes in the new mode

## Usage

1. **Build and flash** the example to your board
2. **Default behavior**: The board starts in **Device mode**
3. **Device mode**:
   - Connect the board to a PC
   - Open a serial terminal (e.g., `screen /dev/ttyACM0` or PuTTY)
   - Type characters - they will be echoed back to you
4. **Switch to Host mode**:
   - Press the board button
   - Connect a USB device to the board
   - The board will enumerate the device and print its descriptors to the debug console
5. **Switch back to Device mode**: Press the button again

## LED Patterns

The onboard LED indicates the USB connection status:

- **Fast blink (250ms)**: Not mounted/connected
- **Slow blink (1000ms)**: Successfully mounted/connected
- **Very slow blink (2500ms)**: Suspended (device mode only)

## Serial Output

The example prints status messages to the debug UART:

```
======================================
TinyUSB Dynamic Switch Example
Press button to switch between device and host modes
Starting in DEVICE mode...
======================================

[DEVICE] Mounted

--- Switching USB mode ---
Stopping DEVICE mode...
Starting HOST mode...
Mode switch complete!

[HOST] Device attached, address = 1
Device 1: ID 1234:5678 SN ABC123
Device Descriptor:
  bLength             18
  bDescriptorType     1
  bcdUSB              0200
  bDeviceClass        239
  ...
```
