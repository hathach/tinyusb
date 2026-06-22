# MSC File Explorer

This host example implements an interactive command-line file browser for USB Mass Storage devices.
When a USB flash drive is connected, the device is automatically mounted using FatFS and a shell-like
CLI is presented over the board's serial console.

## Features

- Automatic mount/unmount of USB storage devices
- FAT12/16/32 filesystem support via FatFS
- Interactive CLI with command history
- Read speed benchmarking with `dd`
- Support for up to 4 simultaneous USB storage devices (via hub)

## Supported Commands

| Command | Usage              | Description                                          |
|---------|--------------------|------------------------------------------------------|
| help    | `help`             | Print list of available commands                     |
| cat     | `cat <file>`       | Print file contents to the console                   |
| cd      | `cd <dir>`         | Change current working directory                     |
| cp      | `cp <src> <dest>`  | Copy a file                                          |
| dd      | `dd [count]`       | Read sectors and report speed (default 1024 sectors) |
| ls      | `ls [dir]`         | List directory contents                              |
| pwd     | `pwd`              | Print current working directory                      |
| mkdir   | `mkdir <dir>`      | Create a directory                                   |
| mv      | `mv <src> <dest>`  | Rename/move a file or directory                      |
| rm      | `rm <file>`        | Remove a file                                        |

## Build

Build for a specific board using CMake (see [Getting Started](https://docs.tinyusb.org/en/latest/getting_started.html)):

```bash
# Example: build for Raspberry Pi Pico
cmake -B build -DBOARD=raspberry_pi_pico -DFAMILY=rp2040 examples/host/msc_file_explorer
cmake --build build
```

## Usage

1. Flash the firmware to your board.
2. Open a serial terminal (e.g. `minicom`, `screen`, `PuTTY`) at 115200 baud.
3. Plug a USB flash drive into the board's USB host port.
4. The device is auto-mounted and the prompt appears:

```
TinyUSB MSC File Explorer Example

Device connected
  Vendor  : Kingston
  Product : DataTraveler 2.0
  Rev     : 1.0
  Capacity: 1.9 GB

0:/> _
```

### Browsing Files

```
0:/> ls
----a    1234  readme.txt
d----       0  photos
d----       0  docs

0:/> cd photos
0:/photos> ls
----a  520432  vacation.jpg
----a  312088  family.png

0:/> cat readme.txt
Hello from USB drive!
```

### Copying and Moving Files

```
0:/> cp readme.txt backup.txt
0:/> mv backup.txt docs/backup.txt
```

### Measuring Read Speed

```
0:/> dd
Reading 1024 sectors...
  Data speed: 823 KB/s
```

### Multiple Devices

When using a USB hub, multiple drives are mounted as `0:`, `1:`, etc. Use the drive prefix to
navigate between them:

```
0:/> cd 1:
1:/> ls
```

## Testing

This example is part of the TinyUSB HIL (Hardware-in-the-Loop) test suite. The HIL test
automatically flashes, runs the example, and verifies MSC enumeration and file operations
against a known USB drive.
