# Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
#
# SPDX-License-Identifier: BSD-3-Clause

# Called after the Raspberry Pi Pico SDK has been initialized to add our libraries

add_library(FreeRTOS-Kernel-Core INTERFACE)
target_sources(FreeRTOS-Kernel-Core INTERFACE
        ${FREERTOS_KERNEL_PATH}/croutine.c
        ${FREERTOS_KERNEL_PATH}/event_groups.c
        ${FREERTOS_KERNEL_PATH}/list.c
        ${FREERTOS_KERNEL_PATH}/queue.c
        ${FREERTOS_KERNEL_PATH}/stream_buffer.c
        ${FREERTOS_KERNEL_PATH}/tasks.c
        ${FREERTOS_KERNEL_PATH}/timers.c
        )
target_include_directories(FreeRTOS-Kernel-Core INTERFACE ${FREERTOS_KERNEL_PATH}/include)

if (PICO_SDK_VERSION_STRING VERSION_GREATER_EQUAL "1.3.2")
    target_compile_definitions(FreeRTOS-Kernel-Core INTERFACE
            PICO_CONFIG_RTOS_ADAPTER_HEADER=${CMAKE_CURRENT_LIST_DIR}/include/freertos_sdk_config.h)
endif()

add_library(FreeRTOS-Kernel INTERFACE)
target_sources(FreeRTOS-Kernel INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/port.c
)

target_include_directories(FreeRTOS-Kernel INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${FREERTOS_CONFIG_FILE_DIRECTORY})

target_link_libraries(FreeRTOS-Kernel INTERFACE
        FreeRTOS-Kernel-Core
        pico_base_headers
        hardware_clocks
        hardware_exception
        pico_multicore
)

target_compile_definitions(FreeRTOS-Kernel INTERFACE
        LIB_FREERTOS_KERNEL=1
        FREE_RTOS_KERNEL_SMP=1
)

add_library(FreeRTOS-Kernel-Static INTERFACE)
target_compile_definitions(FreeRTOS-Kernel-Static INTERFACE
        configSUPPORT_STATIC_ALLOCATION=1
        configKERNEL_PROVIDED_STATIC_MEMORY=1
        )

target_link_libraries(FreeRTOS-Kernel-Static INTERFACE FreeRTOS-Kernel)

add_library(FreeRTOS-Kernel-Heap1 INTERFACE)
target_sources(FreeRTOS-Kernel-Heap1 INTERFACE ${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_1.c)
target_link_libraries(FreeRTOS-Kernel-Heap1 INTERFACE FreeRTOS-Kernel)

add_library(FreeRTOS-Kernel-Heap2 INTERFACE)
target_sources(FreeRTOS-Kernel-Heap2 INTERFACE ${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_2.c)
target_link_libraries(FreeRTOS-Kernel-Heap2 INTERFACE FreeRTOS-Kernel)

add_library(FreeRTOS-Kernel-Heap3 INTERFACE)
target_sources(FreeRTOS-Kernel-Heap3 INTERFACE ${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_3.c)
target_link_libraries(FreeRTOS-Kernel-Heap3 INTERFACE FreeRTOS-Kernel)

add_library(FreeRTOS-Kernel-Heap4 INTERFACE)
target_sources(FreeRTOS-Kernel-Heap4 INTERFACE ${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_4.c)
target_link_libraries(FreeRTOS-Kernel-Heap4 INTERFACE FreeRTOS-Kernel)

add_library(FreeRTOS-Kernel-Heap5 INTERFACE)
target_sources(FreeRTOS-Kernel-Heap5 INTERFACE ${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_5.c)
target_link_libraries(FreeRTOS-Kernel-Heap5 INTERFACE FreeRTOS-Kernel)
