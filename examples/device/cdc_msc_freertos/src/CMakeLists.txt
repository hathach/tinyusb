idf_component_register(SRCS "main.c" "usb_descriptors.c" "msc_disk.c"
                    INCLUDE_DIRS "."
                    REQUIRES boards tinyusb_src)
