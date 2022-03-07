idf_component_register(SRCS "main.c" "usb_descriptors.c"
  INCLUDE_DIRS "."
  REQUIRES freertos soc)

file(TO_NATIVE_PATH "${TOP}/hw/bsp/${FAMILY}/boards/${BOARD}/board.cmake" board_cmake)

if(EXISTS ${board_cmake})
  include(${board_cmake})
endif()

target_include_directories(${COMPONENT_TARGET} PUBLIC
  "${TOP}/hw"
  "${TOP}/src"
)

target_compile_definitions(${COMPONENT_TARGET} PUBLIC
  ESP_PLATFORM
)

target_sources(${COMPONENT_TARGET} PUBLIC
  "${TOP}/src/tusb.c"
  "${TOP}/src/common/tusb_fifo.c"
  "${TOP}/src/device/usbd.c"
  "${TOP}/src/device/usbd_control.c"
  "${TOP}/src/class/audio/audio_device.c"
  "${TOP}/src/portable/espressif/esp32sx/dcd_esp32sx.c"
)