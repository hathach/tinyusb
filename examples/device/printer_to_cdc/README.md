#### Printer to CDC

This example demonstrates a USB composite device with a Printer class interface and a CDC serial interface. Data flows bidirectionally between the two:

- Data sent to the Printer (from host) is forwarded to the CDC serial port
- Data sent to the CDC serial port (from host) is forwarded to the Printer IN endpoint

This is useful for debugging printer class communication or as a reference for implementing printer class devices.

#### USB Interfaces

| Interface | Class | Description |
|-----------|-------|-------------|
| 0 | CDC ACM | Virtual serial port |
| 2 | Printer | USB Printer (bidirectional, protocol 2) |

#### How to Test

The device exposes two endpoints on the host:
- `/dev/ttyACM0` (CDC serial port)
- `/dev/usb/lp0` (USB printer)

Note: the actual device numbers may vary depending on your system.

**Prerequisites (Linux):**

```bash
# Load the USB printer kernel module if not already loaded
sudo modprobe usblp

# Check devices exist
ls /dev/ttyACM* /dev/usb/lp*
```

**Test Printer to CDC (host writes to printer, reads from CDC):**

```bash
# Terminal 1: read from CDC
cat /dev/ttyACM0

# Terminal 2: write to printer
echo "hello from printer" > /dev/usb/lp0
# "hello from printer" appears in Terminal 1
```

**Test CDC to Printer (host writes to CDC, reads from printer):**

```bash
# Terminal 1: read from printer IN endpoint
cat /dev/usb/lp0

# Terminal 2: write to CDC
echo "hello from cdc" > /dev/ttyACM0
# "hello from cdc" appears in Terminal 1
```

**Interactive bidirectional test:**

```bash
# Terminal 1: open CDC serial port
minicom -D /dev/ttyACM0

# Terminal 2: send to printer
echo "tinyusb print example" > /dev/usb/lp0
# Text appears in minicom. Type in minicom to send data back through printer TX.
```

#### IEEE 1284 Device ID

The device responds to GET_DEVICE_ID requests with:

```
MFG:TinyUSB;MDL:Printer to CDC;CMD:PS;CLS:PRINTER;
```

Verify with:

```bash
cat /sys/class/usbmisc/lp0/device/ieee1284_id
```
