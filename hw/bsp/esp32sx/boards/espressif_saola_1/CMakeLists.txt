# Apply board specific context here
target_include_directories(${COMPONENT_LIB} PUBLIC .)

idf_build_get_property(idf_target IDF_TARGET)

if(NOT ${idf_target} STREQUAL "esp32s2")
    message(STATUS "Incorrect target for board ${BOARD}: $ENV{IDF_TARGET} " )
    set(IDF_TARGET "esp32s2" FORCE)
endif()

target_compile_options(${COMPONENT_TARGET} PUBLIC
  "-DCFG_TUSB_MCU=OPT_MCU_ESP32S2"
  "-DCFG_TUSB_OS=OPT_OS_FREERTOS"
)