idf_component_register(SRCS esp32s2.c
                    INCLUDE_DIRS "." "${BOARD}"
                    PRIV_REQUIRES "driver"
                    REQUIRES freertos src led_strip)

# Apply board specific content
include("${BOARD}/board.cmake")

target_include_directories(${COMPONENT_TARGET} PUBLIC
  "${TOP}/hw"
  "${TOP}/src"  
)
