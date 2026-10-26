# This file is for ESP-IDF only
# WHOLE_ARCHIVE: mtp_fs_example.c only overrides weak tud_mtp_*_cb, so the linker would never pull it in
idf_component_register(SRCS "main.c" "mtp_fs_example.c" "usb_descriptors.c"
                    INCLUDE_DIRS "."
                    REQUIRES boards tinyusb_src
                    WHOLE_ARCHIVE)
