# FAMILY = esp32sx
idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES freertos soc)
                    
file(TO_NATIVE_PATH "${TOP}/hw/bsp/${FAMILY}/boards/${BOARD}/board.cmake" board_cmake)

if(EXISTS ${board_cmake})
    include(${board_cmake})
endif()

idf_component_get_property( FREERTOS_ORIG_INCLUDE_PATH freertos ORIG_INCLUDE_PATH)
target_include_directories(${COMPONENT_TARGET} PUBLIC
  "${FREERTOS_ORIG_INCLUDE_PATH}"
  "${TOP}/hw"
  "${TOP}/src"
)
