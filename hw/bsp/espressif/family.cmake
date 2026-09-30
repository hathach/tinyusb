# Apply board specific content i.e IDF_TARGET must be set before project.cmake is included
include("${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake")
string(TOUPPER ${IDF_TARGET} FAMILY_MCUS)

# Device/host port defaults:
# - ESP32-P4 uses Port1 (highspeed)
# - ESP32-S31 uses Port0 (highspeed)
# - Other targets use Port0 and derive the default speed from RHPORT_SPEED
set(RHPORT_SPEED OPT_MODE_FULL_SPEED OPT_MODE_HIGH_SPEED)

if (NOT DEFINED RHPORT_DEVICE)
  if (IDF_TARGET STREQUAL "esp32p4")
    set(RHPORT_DEVICE 1)
  else ()
    set(RHPORT_DEVICE 0)
  endif ()
endif()

if (NOT DEFINED RHPORT_HOST)
  if (IDF_TARGET STREQUAL "esp32p4")
    set(RHPORT_HOST 1)
  else ()
    set(RHPORT_HOST 0)
  endif ()
endif()

if (NOT DEFINED RHPORT_DEVICE_SPEED)
  if (IDF_TARGET STREQUAL "esp32s31")
    set(RHPORT_DEVICE_SPEED OPT_MODE_HIGH_SPEED)
  else ()
    list(GET RHPORT_SPEED ${RHPORT_DEVICE} RHPORT_DEVICE_SPEED)
  endif ()
endif ()
if (NOT DEFINED RHPORT_HOST_SPEED)
  if (IDF_TARGET STREQUAL "esp32s31")
    set(RHPORT_HOST_SPEED OPT_MODE_HIGH_SPEED)
  else ()
    list(GET RHPORT_SPEED ${RHPORT_HOST} RHPORT_HOST_SPEED)
  endif ()
endif ()

# Add example src and bsp directories
set(EXTRA_COMPONENT_DIRS "src" "${CMAKE_CURRENT_LIST_DIR}/boards" "${CMAKE_CURRENT_LIST_DIR}/components")
set(SDKCONFIG ${CMAKE_BINARY_DIR}/sdkconfig)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
