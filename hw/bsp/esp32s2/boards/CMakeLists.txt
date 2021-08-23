idf_component_register(SRCS esp32s2.c
                    INCLUDE_DIRS "." "${BOARD}"
                    PRIV_REQUIRES "driver"
                    REQUIRES freertos src led_strip)

# Apply board specific content
include("${BOARD}/board.cmake")

idf_component_get_property( FREERTOS_ORIG_INCLUDE_PATH freertos ORIG_INCLUDE_PATH)
target_include_directories(${COMPONENT_TARGET} PUBLIC
  "${FREERTOS_ORIG_INCLUDE_PATH}"
  "${TOP}/hw"
  "${TOP}/src"  
)
