cmake_minimum_required(VERSION 3.20)

# Also allow running the host fixtures without installing Ceedling.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(tinyusb_host_tests LANGUAGES C)
  enable_testing()
endif ()

if (NOT CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
  message(FATAL_ERROR "Host fixtures require GCC or Clang (MinGW on Windows).")
endif ()

get_filename_component(TINYUSB_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

function(add_host_test name fixture)
  add_executable(${name} ${fixture})
  get_filename_component(fixture_dir "${fixture}" DIRECTORY)
  target_include_directories(${name} PRIVATE ${fixture_dir} ${TINYUSB_ROOT}/src)
  set_target_properties(${name} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)
  # Keep assertions active even when configured with a release build type.
  target_compile_options(${name} PRIVATE -O2 -g -Wall -Wextra -Werror -UNDEBUG)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES TIMEOUT 10 LABELS host)
endfunction()

foreach (speed IN ITEMS high full)
  # hub 2 also covers a full-speed hub behind the high-speed hub that owns the TT
  foreach (hub IN ITEMS 0 1 2)
    set(name test_wch_usbhs_v1_${speed}_hub${hub})
    add_host_test(${name} ${CMAKE_CURRENT_LIST_DIR}/wch_usbhs_v1/test_hcd.c)
    target_compile_definitions(${name} PRIVATE CFG_TUH_HUB=${hub})
    if (speed STREQUAL "full")
      target_compile_definitions(${name} PRIVATE CFG_TUH_MAX_SPEED=OPT_MODE_FULL_SPEED)
    endif ()
  endforeach ()
endforeach ()

# Speed selection must work without BSP-provided BOARD_TUx_MAX_SPEED defaults.
foreach (device_hs IN ITEMS 0 1)
  math(EXPR host_hs "1 - ${device_hs}")
  foreach (mode IN ITEMS default auto explicit legacy)
    set(name test_wch_usbhs_v1_speed_${device_hs}_${mode})
    add_host_test(${name} ${CMAKE_CURRENT_LIST_DIR}/wch_usbhs_v1/test_speed.c)
    target_compile_definitions(${name} PRIVATE
      "CFG_TUSB_CONFIG_FILE=<stdint.h>"
      CFG_TUSB_MCU=OPT_MCU_CH32V307 CFG_TUSB_OS=OPT_OS_NONE
      CFG_TUD_ENABLED=1 CFG_TUH_ENABLED=1
      CFG_TUD_WCH_USBIP_USBHS=${device_hs} CFG_TUD_WCH_USBIP_USBFS=${host_hs}
      CFG_TUH_WCH_USBIP_USBHS=${host_hs} CFG_TUH_WCH_USBIP_USBFS=${device_hs}
      )
    if (mode STREQUAL "default" OR mode STREQUAL "auto")
      target_compile_definitions(${name} PRIVATE EXPECT_DEVICE_HS=${device_hs} EXPECT_HOST_HS=${host_hs})
      if (mode STREQUAL "auto")
        target_compile_definitions(${name} PRIVATE
          CFG_TUD_MAX_SPEED=OPT_MODE_DEFAULT_SPEED CFG_TUH_MAX_SPEED=OPT_MODE_DEFAULT_SPEED)
      endif ()
    else ()
      target_compile_definitions(${name} PRIVATE EXPECT_DEVICE_HS=0 EXPECT_HOST_HS=0)
      if (mode STREQUAL "explicit")
        target_compile_definitions(${name} PRIVATE
          CFG_TUD_MAX_SPEED=OPT_MODE_FULL_SPEED CFG_TUH_MAX_SPEED=OPT_MODE_FULL_SPEED)
      else ()
        target_compile_definitions(${name} PRIVATE
          "CFG_TUSB_RHPORT0_MODE=OPT_MODE_DEVICE|OPT_MODE_FULL_SPEED"
          "CFG_TUSB_RHPORT1_MODE=OPT_MODE_HOST|OPT_MODE_FULL_SPEED")
      endif ()
    endif ()
  endforeach ()
endforeach ()

add_host_test(test_stm32_fsdev_setup ${CMAKE_CURRENT_LIST_DIR}/stm32_fsdev/test_setup.c)

# EHCI fixture links encode native pointers in 32 bits. macOS cannot guarantee
# low static addresses (in particular, Apple Silicon executables require PIE).
if (APPLE)
  message(STATUS "Skipping EHCI fixtures on macOS: static DMA addresses must fit in 32 bits.")
  # Register a skipped placeholder so CTest reports the skip instead of an
  # empty, silently passing run.
  add_test(NAME test_ehci_iso_skipped_on_macos COMMAND ${CMAKE_COMMAND} -E false)
  set_tests_properties(test_ehci_iso_skipped_on_macos PROPERTIES SKIP_RETURN_CODE 1 LABELS host)
  return()
endif ()

foreach (variant IN ITEMS generic disabled enabled cached nohub)
  set(name test_ehci_iso_${variant})
  add_host_test(${name} ${CMAKE_CURRENT_LIST_DIR}/ehci/test_iso.c)
  target_sources(${name} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ehci/cache_stubs.c)
  if (NOT variant STREQUAL "disabled")
    target_compile_definitions(${name} PRIVATE CFG_TUH_CHIPIDEA_ISO_ENABLE=1)
  endif ()
  if (variant STREQUAL "enabled" OR variant STREQUAL "cached")
    target_sources(${name} PRIVATE ${TINYUSB_ROOT}/src/host/hub.c)
  endif ()
  if (variant STREQUAL "nohub")
    target_compile_definitions(${name} PRIVATE CFG_TUH_HUB=0)
  endif ()
  if (variant STREQUAL "cached")
    target_compile_definitions(${name} PRIVATE CFG_TUH_MEM_DCACHE_ENABLE=1)
  endif ()
  if (variant STREQUAL "generic")
    target_compile_definitions(${name} PRIVATE TEST_EHCI_GENERIC)
  endif ()
  target_compile_options(${name} PRIVATE -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast)
  # EHCI hardware links are 32-bit; keep simulated DMA memory below 4 GiB.
  if (WIN32)
    target_link_options(${name} PRIVATE -Wl,--image-base,0x400000,--disable-dynamicbase)
  else ()
    target_compile_options(${name} PRIVATE -fno-pie)
    target_link_options(${name} PRIVATE -no-pie)
  endif ()
endforeach ()
