include_guard()

include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

set(FAMILY_MCUS LINUX_RAW_GADGET CACHE INTERNAL "")

function(family_add_board BOARD_TARGET)
  add_library(${BOARD_TARGET} INTERFACE)

  target_include_directories(${BOARD_TARGET} INTERFACE
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    ${TOP}/hw
  )

  update_board(${BOARD_TARGET})
endfunction()

function(family_configure_example TARGET RTOS)
  if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR
      "The Linux Raw Gadget port can only be built on Linux"
    )
  endif()

  if (NOT RTOS STREQUAL "noos")
    return()
  endif()

  find_package(Threads REQUIRED)

  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_LINUX_RAW_GADGET)

  target_sources(${TARGET} PRIVATE
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${TOP}/hw/bsp/board.c

    ${TOP}/src/portable/linux/raw_gadget/dcd_raw_gadget.c
    ${TOP}/src/portable/linux/raw_gadget/raw_gadget_context.c
    ${TOP}/src/portable/linux/raw_gadget/raw_gadget_endpoint.c
    ${TOP}/src/portable/linux/raw_gadget/raw_gadget_event.c
    ${TOP}/src/portable/linux/raw_gadget/raw_gadget_hal.c
    ${TOP}/src/portable/linux/raw_gadget/raw_gadget_transfer.c
    ${TOP}/src/portable/linux/raw_gadget/raw_gadget_udc.c
  )

  target_link_libraries(${TARGET} PRIVATE
    Threads::Threads
  )
endfunction()
