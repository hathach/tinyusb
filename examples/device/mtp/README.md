# MTP

A USB Media Transfer Protocol device backed by a small in-RAM filesystem that the host can browse, read from and write to.

## What it does

- Presents a single MTP storage preloaded with two read-only objects: `readme.txt` and `tinyusb.png`.
- Supports core MTP operations: get device info, open/close session, get storage IDs and storage info, enumerate object handles, get object info, get object and get partial object, and a device property (device friendly name).
- Lets the host upload one additional object (SendObjectInfo / SendObject) into a 4 KB RAM buffer and delete objects (unless built read-only).
- Handles MTP class control requests: cancel, device reset and get device status.
- LED blink rate indicates bus state (not mounted / mounted / suspended).

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0 | MTP |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_MTP                     1
#define CFG_TUD_MTP_EP_BUFSIZE          512
#define CFG_TUD_MTP_EP_CONTROL_BUFSIZE  16 // should be enough to hold data in MTP control request

// MTP device info
#define CFG_TUD_MTP_DEVICEINFO_EXTENSIONS   "microsoft.com: 1.0; "
#define CFG_TUD_MTP_DEVICEINFO_SUPPORTED_OPERATIONS \
   MTP_OP_GET_DEVICE_INFO, \
   MTP_OP_OPEN_SESSION, \
   MTP_OP_CLOSE_SESSION, \
   MTP_OP_GET_STORAGE_IDS, \
   MTP_OP_GET_STORAGE_INFO, \
   MTP_OP_GET_OBJECT_HANDLES, \
   MTP_OP_GET_OBJECT_INFO, \
   MTP_OP_GET_OBJECT, \
   MTP_OP_GET_PARTIAL_OBJECT, \
   MTP_OP_DELETE_OBJECT, \
   MTP_OP_SEND_OBJECT_INFO, \
   MTP_OP_SEND_OBJECT, \
   MTP_OP_RESET_DEVICE, \
   MTP_OP_GET_DEVICE_PROP_DESC, \
   MTP_OP_GET_DEVICE_PROP_VALUE, \
   MTP_OP_SET_DEVICE_PROP_VALUE

#define CFG_TUD_MTP_DEVICEINFO_SUPPORTED_EVENTS \
    MTP_EVENT_OBJECT_ADDED

#define CFG_TUD_MTP_DEVICEINFO_SUPPORTED_DEVICE_PROPERTIES  \
    MTP_DEV_PROP_DEVICE_FRIENDLY_NAME

#define CFG_TUD_MTP_DEVICEINFO_CAPTURE_FORMATS \
    MTP_OBJ_FORMAT_UNDEFINED, \
    MTP_OBJ_FORMAT_ASSOCIATION, \
    MTP_OBJ_FORMAT_TEXT, \
    MTP_OBJ_FORMAT_PNG

#define CFG_TUD_MTP_DEVICEINFO_PLAYBACK_FORMATS \
    MTP_OBJ_FORMAT_UNDEFINED, \
    MTP_OBJ_FORMAT_ASSOCIATION, \
    MTP_OBJ_FORMAT_TEXT, \
    MTP_OBJ_FORMAT_PNG
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

The device enumerates as an MTP device and shows up in a file browser (e.g. the Files/Explorer app, or `mtp-detect` / `mtp-files` from libmtp). You should see `readme.txt` and `tinyusb.png`; you can copy a small file onto the device and delete files.
