# Install python3 HID package https://pypi.org/project/hid/
import hid

USB_VID = 0xcafe

for dict in hid.enumerate(0xcafe):
    print(dict)
    dev = hid.Device(dict['vendor_id'], dict['product_id'])
    if dev:
        while True:
            # Get input from console and encode to UTF8 for array of chars.
            str_out = input("Send text to HID Device : ").encode('utf-8')
            dev.write(str_out)
            str_in = dev.read(64)
            print("Received from HID Device:", str_in, '\n')
