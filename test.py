from winusbcdc.winusbpy import *
from winusbcdc.winusbclasses import *

api = WinUsbPy()
result = api.list_usb_devices(deviceinterface=True, present=True)
if result:
  if api.init_winusb_device(None, 0xcafe, 0x4020):
    print("found")
    api.control_transfer(UsbSetupPacket(0x00, 0x03, 0x2, 0x04 << 8, 0))
