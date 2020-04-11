idf_component_register(SRCS "esp32s2_saola_1.c" "led_strip/src/led_strip_rmt_ws2812.c"
                    INCLUDE_DIRS "led_strip/include"
                    PRIV_REQUIRES "driver"
                    REQUIRES freertos src)

target_compile_options(${COMPONENT_TARGET} PUBLIC
  "-DCFG_TUSB_MCU=OPT_MCU_ESP32S2"
  "-DCFG_TUSB_OS=OPT_OS_FREERTOS"
)

idf_component_get_property( FREERTOS_ORIG_INCLUDE_PATH freertos ORIG_INCLUDE_PATH)
target_include_directories(${COMPONENT_TARGET} PUBLIC
  "${FREERTOS_ORIG_INCLUDE_PATH}"
  "${TOP}/hw"
  "${TOP}/src"  
)
