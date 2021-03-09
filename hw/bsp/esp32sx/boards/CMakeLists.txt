idf_component_register(SRCS esp32sx.c
                    INCLUDE_DIRS "." "${BOARD}"
                    PRIV_REQUIRES "driver"
                    REQUIRES freertos src led_strip)

# Apply board specific content
add_subdirectory(${BOARD})

idf_component_get_property( FREERTOS_ORIG_INCLUDE_PATH freertos ORIG_INCLUDE_PATH)
target_include_directories(${COMPONENT_TARGET} PUBLIC
  "${FREERTOS_ORIG_INCLUDE_PATH}"
  "${TOP}/hw"
  "${TOP}/src"  
)
