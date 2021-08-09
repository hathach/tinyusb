# Install python3 HID package https://pypi.org/project/hid/
import hid

USB_VID = 0xcafe

print("Openning HID device with VID = 0x%X" % USB_VID)

for dict in hid.enumerate(USB_VID):
    print(dict)
    dev = hid.Device(dict['vendor_id'], dict['product_id'])
    if dev:
        while True:
            # Get input from console and encode to UTF8 for array of chars.
            # hid generic inout is single report therefore by HIDAPI requirement
            # it must be preceeded with 0x00 as dummy reportID
            str_out = b'\x00'
            str_out += input("Send text to HID Device : ").encode('utf-8')
            dev.write(str_out)
            str_in = dev.read(64)
            print("Received from HID Device:", str_in, '\n')
