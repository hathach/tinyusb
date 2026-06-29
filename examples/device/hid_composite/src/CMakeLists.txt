idf_component_register(SRCS "main.c" "usb_descriptors.c"
                    INCLUDE_DIRS "."
                    REQUIRES boards tinyusb_src)
