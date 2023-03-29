cmake_minimum_required(VERSION 3.5)

# TOP is absolute path to root directory of TinyUSB git repo
set(TOP "../../..")
get_filename_component(TOP "${TOP}" REALPATH)

# Check for -DFAMILY=
if(FAMILY MATCHES "^esp32s[2-3]")
  # use BOARD-Directory name for project id
  get_filename_component(PROJECT ${CMAKE_CURRENT_SOURCE_DIR} NAME)
  set(PROJECT ${BOARD}-${PROJECT})
  include(${TOP}/hw/bsp/${FAMILY}/family.cmake)
  project(${PROJECT})
elseif(FAMILY MATCHES "rp2040")
  # gets PROJECT name for the example (e.g. <BOARD>-<DIR_NAME>)
  family_get_project_name(PROJECT ${CMAKE_CURRENT_LIST_DIR})
  if (DEFINED ENV{FREERTOS_KERNEL_PATH})
  # Pull in FreeRTOS
  include(FreeRTOS_Kernel_import.cmake)
  project(${PROJECT})
  include(${TOP}/hw/bsp/${FAMILY}/family.cmake)
  # Checks this example is valid for the family and initializes the project
  family_initialize_project(${PROJECT} ${CMAKE_CURRENT_LIST_DIR})

  add_executable(${PROJECT})
  # Example source
  target_sources(${PROJECT} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/usb_descriptors.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/freertos_hook.c
        )

  # Example include
  target_include_directories(${PROJECT} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/FreeRTOSConfig
        )
  if (PICO_C_COMPILER_IS_GNU)
    # TODO: watch for update to the FreeRTOS-Kernel code
    # There are a disturbing number of warnings in the FreeRTOS-Kernel smp branch
    # The need for -Wno-shadow -Wno-cast-qual -Wno-cast-align indicate likely bugs
    set_source_files_properties(
            ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/port.c
            ${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_4.c
            ${FREERTOS_KERNEL_PATH}/event_groups.c
            ${FREERTOS_KERNEL_PATH}/croutine.c
            ${FREERTOS_KERNEL_PATH}/timers.c
            ${FREERTOS_KERNEL_PATH}/queue.c
            ${FREERTOS_KERNEL_PATH}/tasks.c
            ${FREERTOS_KERNEL_PATH}/list.c
            ${FREERTOS_KERNEL_PATH}/stream_buffer.c
            PROPERTIES
            COMPILE_FLAGS "-Wno-type-limits -Wno-conversion -Wno-sign-compare -Wno-shadow -Wno-unused-parameter -Wno-cast-qual -Wno-cast-align")
    # Small issues in vPortRecursiveLock() in portmacro.h cause the following warnings in our added code
    set_source_files_properties(
            ${PICO_TINYUSB_PATH}/hw/bsp/rp2040/family.c
            ${PICO_TINYUSB_PATH}/src/tusb.c
            ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/rp2040/dcd_rp2040.c
            ${PICO_TINYUSB_PATH}/src/class/hid/hid_device.c
            ${PICO_TINYUSB_PATH}/src/common/tusb_fifo.c
            ${PICO_TINYUSB_PATH}/src/device/usbd.c
            ${PICO_TINYUSB_PATH}/src/device/usbd_control.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/usb_descriptors.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/freertos_hook.c
            PROPERTIES
            COMPILE_FLAGS "-Wno-type-limits -Wno-conversion")
  endif()
  # Configure compilation flags and libraries for the example... see the corresponding function
  # in hw/bsp/FAMILY/family.cmake for details.
  family_configure_device_freertos_example(${PROJECT})
  else()
    message(STATUS "skipping ${PROJECT}: FREERTOS_KERNEL_PATH is not defined")
  endif()
else()
  message(FATAL_ERROR "Invalid FAMILY specified: ${FAMILY}")
endif()
