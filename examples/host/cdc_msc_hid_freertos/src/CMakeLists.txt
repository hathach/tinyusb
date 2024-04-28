# This file is for ESP-IDF only
idf_component_register(SRCS "cdc_app.c" "hid_app.c" "main.c" "msc_app.c"
                    INCLUDE_DIRS "."
                    REQUIRES boards tinyusb_src)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=format)
