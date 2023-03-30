idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES freertos soc tinyusb_src)

target_include_directories(${COMPONENT_TARGET} PUBLIC
    "${TOP}/hw"
    )
