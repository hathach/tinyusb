cmake_minimum_required(VERSION 3.5)

set(TOP "../../..")
get_filename_component(TOP "${TOP}" REALPATH)

if (EXISTS ${TOP}/lib/lwip/src)
    include(${TOP}/hw/bsp/${FAMILY}/family.cmake)

    # gets PROJECT name for the example (e.g. <BOARD>-<DIR_NAME>)
    family_get_project_name(PROJECT ${CMAKE_CURRENT_LIST_DIR})

    project(${PROJECT})

    # Checks this example is valid for the family and initializes the project
    family_initialize_project(${PROJECT} ${CMAKE_CURRENT_LIST_DIR})

    add_executable(${PROJECT})

    # Example source
    target_sources(${PROJECT} PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/usb_descriptors.c
            )

    # Example include
    target_include_directories(${PROJECT} PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${TOP}/lib/lwip/src/include
            ${TOP}/lib/lwip/src/include/ipv4
            ${TOP}/lib/lwip/src/include/lwip/apps
            ${TOP}/lib/networking
            )

    target_sources(${PROJECT} PUBLIC
            ${TOP}/lib/lwip/src/core/altcp.c
            ${TOP}/lib/lwip/src/core/altcp_alloc.c
            ${TOP}/lib/lwip/src/core/altcp_tcp.c
            ${TOP}/lib/lwip/src/core/def.c
            ${TOP}/lib/lwip/src/core/dns.c
            ${TOP}/lib/lwip/src/core/inet_chksum.c
            ${TOP}/lib/lwip/src/core/init.c
            ${TOP}/lib/lwip/src/core/ip.c
            ${TOP}/lib/lwip/src/core/mem.c
            ${TOP}/lib/lwip/src/core/memp.c
            ${TOP}/lib/lwip/src/core/netif.c
            ${TOP}/lib/lwip/src/core/pbuf.c
            ${TOP}/lib/lwip/src/core/raw.c
            ${TOP}/lib/lwip/src/core/stats.c
            ${TOP}/lib/lwip/src/core/sys.c
            ${TOP}/lib/lwip/src/core/tcp.c
            ${TOP}/lib/lwip/src/core/tcp_in.c
            ${TOP}/lib/lwip/src/core/tcp_out.c
            ${TOP}/lib/lwip/src/core/timeouts.c
            ${TOP}/lib/lwip/src/core/udp.c
            ${TOP}/lib/lwip/src/core/ipv4/autoip.c
            ${TOP}/lib/lwip/src/core/ipv4/dhcp.c
            ${TOP}/lib/lwip/src/core/ipv4/etharp.c
            ${TOP}/lib/lwip/src/core/ipv4/icmp.c
            ${TOP}/lib/lwip/src/core/ipv4/igmp.c
            ${TOP}/lib/lwip/src/core/ipv4/ip4.c
            ${TOP}/lib/lwip/src/core/ipv4/ip4_addr.c
            ${TOP}/lib/lwip/src/core/ipv4/ip4_frag.c
            ${TOP}/lib/lwip/src/netif/ethernet.c
            ${TOP}/lib/lwip/src/netif/slipif.c
            ${TOP}/lib/lwip/src/apps/http/httpd.c
            ${TOP}/lib/lwip/src/apps/http/fs.c
            ${TOP}/lib/networking/dhserver.c
            ${TOP}/lib/networking/dnserver.c
            ${TOP}/lib/networking/rndis_reports.c
            )

    target_compile_definitions(${PROJECT} PUBLIC
            PBUF_POOL_SIZE=2
            TCP_WND=2*TCP_MSS
            HTTPD_USE_CUSTOM_FSDATA=0
            )

    # Configure compilation flags and libraries for the example... see the corresponding function
    # in hw/bsp/FAMILY/family.cmake for details.
    family_configure_device_example(${PROJECT})
endif()