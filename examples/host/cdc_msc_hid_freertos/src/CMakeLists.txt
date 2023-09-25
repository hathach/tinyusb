# This file is for ESP-IDF only
idf_component_register(SRCS "hid_app.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES boards tinyusb_src)
