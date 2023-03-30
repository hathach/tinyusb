idf_component_register(SRCS family.c
                    INCLUDE_DIRS "." "${BOARD}"
                    PRIV_REQUIRES "driver"
                    REQUIRES led_strip src tinyusb_src)

target_include_directories(${COMPONENT_TARGET} PUBLIC
  "${TOP}/hw"
)
