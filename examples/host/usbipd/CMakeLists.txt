cmake_minimum_required(VERSION 3.20)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../hw/bsp/family_support.cmake)

# gets PROJECT name for the example (e.g. <BOARD>-<DIR_NAME>)
family_get_project_name(PROJECT ${CMAKE_CURRENT_LIST_DIR})

# lwIP comes from the bundled tinyusb submodule (matches the
# net_lwip_webserver device example pattern).
set(LWIP ${TOP}/lib/lwip)
if (NOT EXISTS ${LWIP}/src)
  set(LWIP ${TINYUSB_LWIP_PATH})
endif()
if (NOT EXISTS ${LWIP}/src)
  family_example_missing_dependency(${PROJECT} "lib/lwip")
  return()
endif()

project(${PROJECT} C CXX ASM)

# Checks this example is valid for the family and initializes the project
family_initialize_project(${PROJECT} ${CMAKE_CURRENT_LIST_DIR})

# Espressif has its own cmake build system; not supported in this example.
if(FAMILY STREQUAL "espressif")
  return()
endif()

add_executable(${PROJECT})

# Source files for the example. main.c drives tuh_task() +
# ethernetif_input() + sys_check_timeouts() in a single loop. The
# usbip_*.c files split the bridge into: server (TCP callbacks +
# framing), devices (cache + DEVLIST/IMPORT), inflight (URB pool +
# completion), submit (CMD_SUBMIT/UNLINK dispatch).
target_sources(${PROJECT} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/usbip_server.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/usbip_devices.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/usbip_inflight.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/usbip_submit.c
  )

target_include_directories(${PROJECT} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${LWIP}/src/include
  )

# Pull in the family's Ethernet support: MAC driver, PHY driver and
# the lwIP netif glue, plus the board's PHY pin map. Defined per
# family in hw/bsp/<family>/family.cmake.
family_add_eth(${PROJECT})

# lwIP raw API + DHCP. Mirrors the net_lwip_webserver source list,
# minus the device-side networking helpers (dhserver, etc).
target_sources(${PROJECT} PUBLIC
  ${LWIP}/src/core/def.c
  ${LWIP}/src/core/inet_chksum.c
  ${LWIP}/src/core/init.c
  ${LWIP}/src/core/ip.c
  ${LWIP}/src/core/mem.c
  ${LWIP}/src/core/memp.c
  ${LWIP}/src/core/netif.c
  ${LWIP}/src/core/pbuf.c
  ${LWIP}/src/core/raw.c
  ${LWIP}/src/core/stats.c
  ${LWIP}/src/core/sys.c
  ${LWIP}/src/core/tcp.c
  ${LWIP}/src/core/tcp_in.c
  ${LWIP}/src/core/tcp_out.c
  ${LWIP}/src/core/timeouts.c
  ${LWIP}/src/core/udp.c
  ${LWIP}/src/core/ipv4/autoip.c
  ${LWIP}/src/core/ipv4/dhcp.c
  ${LWIP}/src/core/ipv4/etharp.c
  ${LWIP}/src/core/ipv4/icmp.c
  ${LWIP}/src/core/ipv4/igmp.c
  ${LWIP}/src/core/ipv4/ip4.c
  ${LWIP}/src/core/ipv4/ip4_addr.c
  ${LWIP}/src/core/ipv4/ip4_frag.c
  ${LWIP}/src/netif/ethernet.c
  )

# All TinyUSB class drivers are disabled in tusb_config.h - this bridge
# forwards URBs at the endpoint level. BUILTIN_DRIVER_COUNT==0 trips
# upstream's -Wtype-limits in usbh.c (drv_id < 0 always-false). Silence
# locally; the example's own translation units stay strict.
target_compile_options(${PROJECT} PRIVATE -Wno-type-limits)

# lwIP carries its own warning hygiene that doesn't match TinyUSB's
# strict baseline. Scope the suppressions per source file so the
# example's own code (main.c, usbip_server.c) compiles cleanly under
# the family defaults. family_add_eth applies its own suppressions
# to the BSP-side vendored sources.
set(USBIPD_VENDOR_FILES "")
foreach(LWIPSRC IN ITEMS
    core/def.c core/inet_chksum.c core/init.c core/ip.c core/mem.c
    core/memp.c core/netif.c core/pbuf.c core/raw.c core/stats.c
    core/sys.c core/tcp.c core/tcp_in.c core/tcp_out.c core/timeouts.c
    core/udp.c core/ipv4/autoip.c core/ipv4/dhcp.c core/ipv4/etharp.c
    core/ipv4/icmp.c core/ipv4/igmp.c core/ipv4/ip4.c
    core/ipv4/ip4_addr.c core/ipv4/ip4_frag.c netif/ethernet.c)
  list(APPEND USBIPD_VENDOR_FILES ${LWIP}/src/${LWIPSRC})
endforeach()
set_source_files_properties(${USBIPD_VENDOR_FILES} PROPERTIES
    COMPILE_FLAGS
    "-Wno-error=conversion -Wno-error=sign-conversion -Wno-error=sign-compare -Wno-error=unused-parameter -Wno-error=cast-align -Wno-error=redundant-decls -Wno-error=missing-prototypes")

# Configure compilation flags and libraries for the example, no-RTOS.
family_configure_host_example(${PROJECT} noos)
