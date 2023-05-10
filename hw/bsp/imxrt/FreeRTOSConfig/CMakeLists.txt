if (NOT TARGET freertos_config)
  add_library(freertos_config INTERFACE)

  # add path to FreeRTOSConfig.h
  target_include_directories(freertos_config SYSTEM INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    )
endif()
