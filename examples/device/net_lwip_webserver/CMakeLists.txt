cmake_minimum_required(VERSION 3.20)

include(${CMAKE_CURRENT_LIST_DIR}/../../../hw/bsp/family_support.cmake)

# gets PROJECT name for the example (e.g. <BOARD>-<DIR_NAME>)
family_get_project_name(PROJECT ${CMAKE_CURRENT_LIST_DIR})

# Prefer the tinyusb lwip
set(LWIP ${TOP}/lib/lwip)

# If we can't find one from tinyusb then check cmake var before giving up
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

add_executable(${PROJECT})

# Example source
target_sources(${PROJECT} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/src/main.c
  ${CMAKE_CURRENT_LIST_DIR}/src/usb_descriptors.c
  )

# Example include
target_include_directories(${PROJECT} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/src
  ${LWIP}/src/include
  ${LWIP}/src/include/ipv4
  ${LWIP}/src/include/lwip/apps
  ${TOP}/lib/networking
  )

# lib/networking sources
target_sources(${PROJECT} PUBLIC
  ${TOP}/lib/networking/dhserver.c
  ${TOP}/lib/networking/dnserver.c
  ${TOP}/lib/networking/rndis_reports.c
  )

# lwip sources
target_sources(${PROJECT} PUBLIC
  ${LWIP}/src/core/altcp.c
  ${LWIP}/src/core/altcp_alloc.c
  ${LWIP}/src/core/altcp_tcp.c
  ${LWIP}/src/core/def.c
  ${LWIP}/src/core/dns.c
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
  ${LWIP}/src/netif/slipif.c
  ${LWIP}/src/apps/http/httpd.c
  ${LWIP}/src/apps/http/fs.c
  ${LWIP}/src/apps/lwiperf/lwiperf.c
  )

# due to warnings from other net source, we need to prevent error from some of the warnings options
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
  target_compile_options(${PROJECT} PUBLIC
    -Wno-error=null-dereference
    -Wno-error=conversion
    -Wno-error=sign-conversion
    -Wno-error=sign-compare
    )
elseif (CMAKE_C_COMPILER_ID STREQUAL "IAR")

endif ()

# Configure compilation flags and libraries for the example without RTOS.
# See the corresponding function in hw/bsp/FAMILY/family.cmake for details.
family_configure_device_example(${PROJECT} noos)
