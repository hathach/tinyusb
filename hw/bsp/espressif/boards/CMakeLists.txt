set(hw_dir "${CMAKE_CURRENT_LIST_DIR}/../../../")

idf_component_register(SRCS family.c
                    INCLUDE_DIRS "." ${BOARD} ${hw_dir}
                    PRIV_REQUIRES "driver"
                    REQUIRES led_strip src tinyusb_src)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=format)
