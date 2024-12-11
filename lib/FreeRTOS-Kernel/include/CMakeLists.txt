# FreeRTOS internal cmake file. Do not use it in user top-level project

add_library(freertos_kernel_include INTERFACE)

target_include_directories(freertos_kernel_include
    INTERFACE
        .
        # Note: DEPRECATED but still supported, may be removed in a future release.
        $<$<NOT:$<TARGET_EXISTS:freertos_config>>:${FREERTOS_CONFIG_FILE_DIRECTORY}>
)

target_link_libraries(freertos_kernel_include
    INTERFACE
        $<$<TARGET_EXISTS:freertos_config>:freertos_config>
)
