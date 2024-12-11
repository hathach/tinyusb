cmake_minimum_required(VERSION 3.15)

project(coverity)

set(FREERTOS_KERNEL_PATH "../..")
FILE(GLOB FREERTOS_KERNEL_SOURCE ${FREERTOS_KERNEL_PATH}/*.c)
FILE(GLOB FREERTOS_PORT_CODE ${FREERTOS_KERNEL_PATH}/portable/template/*.c)

# Coverity incorrectly infers the type of pdTRUE and pdFALSE as boolean because
# of their names. This generates multiple false positive warnings about type
# mismatch. Replace pdTRUE with pdPASS and pdFALSE with pdFAIL to avoid these
# false positive warnings. This workaround will not be needed after Coverity
# fixes the issue of incorrectly inferring the type of pdTRUE and pdFALSE as
# boolean.
add_custom_target(fix_source ALL
                  COMMAND sed -i -b -e 's/pdFALSE/pdFAIL/g' -e 's/pdTRUE/pdPASS/g' ${FREERTOS_KERNEL_SOURCE} ${FREERTOS_PORT_CODE}
                  DEPENDS ${FREERTOS_KERNEL_SOURCE} ${FREERTOS_PORT_CODE})

# Add the freertos_config for FreeRTOS-Kernel.
add_library(freertos_config INTERFACE)

target_include_directories(freertos_config
                           INTERFACE
                           ./)

if (DEFINED FREERTOS_SMP_EXAMPLE AND FREERTOS_SMP_EXAMPLE STREQUAL "1")
    message(STATUS "Build FreeRTOS SMP example")
    # Adding the following configurations to build SMP template port
    add_compile_options( -DconfigNUMBER_OF_CORES=2 -DconfigUSE_PASSIVE_IDLE_HOOK=0 )
endif()

# Select the heap. Values between 1-5 will pick a heap.
set(FREERTOS_HEAP "3" CACHE STRING "" FORCE)

# Select the FreeRTOS port.
set(FREERTOS_PORT "TEMPLATE" CACHE STRING "" FORCE)

# Add the FreeRTOS-Kernel subdirectory.
add_subdirectory(${FREERTOS_KERNEL_PATH} FreeRTOS-Kernel)

add_executable(${PROJECT_NAME}
               ../cmake_example/main.c)

add_dependencies(${PROJECT_NAME} fix_source)

target_link_libraries(${PROJECT_NAME} freertos_kernel freertos_config)
