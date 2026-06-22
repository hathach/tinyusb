include_guard()

# Infineon PSOC Edge E84 (PSE84) family - High-Speed DWC2 USB
# Currently supports the secure (S) Cortex-M33 partition under Zephyr only.

set(MAX3421_HOST OFF)

# include board specific, for zephyr BOARD_ALIAS may be used instead
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake OPTIONAL RESULT_VARIABLE board_cmake_included)
if (NOT board_cmake_included)
  include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD_ALIAS}/board.cmake)
endif ()

set(FAMILY_MCUS PSE84 CACHE INTERNAL "")

if (NOT RTOS STREQUAL zephyr)
  message(FATAL_ERROR "FAMILY=pse84 currently only supports -DRTOS=zephyr")
endif ()

#------------------------------------
# Board Target (zephyr provides startup/linker/HAL)
#------------------------------------
function(family_add_board BOARD_TARGET)
  # Board sources are provided by Zephyr; nothing to add here.
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})
  family_add_tinyusb(${TARGET} OPT_MCU_PSE84)

  target_sources(${TARGET} PRIVATE
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/synopsys/dwc2/dcd_dwc2.c
    ${TOP}/src/portable/synopsys/dwc2/hcd_dwc2.c
    ${TOP}/src/portable/synopsys/dwc2/dwc2_common.c
    )

  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )
endfunction()
