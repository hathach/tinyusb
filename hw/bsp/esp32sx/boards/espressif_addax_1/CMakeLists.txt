# Apply board specific content here
target_include_directories(${COMPONENT_LIB} PRIVATE .)

message(STATUS "Apply board specific content ${BOARD}: $ENV{IDF_TARGET} " )
idf_build_get_property(idf_target IDF_TARGET)

if(NOT ${idf_target} STREQUAL "esp32s3")
    message(STATUS "Incorrect target for board ${BOARD}: $ENV{IDF_TARGET} " )
    set(IDF_TARGET "esp32s3" FORCE)
endif()

target_compile_options(${COMPONENT_TARGET} PUBLIC
  "-DCFG_TUSB_MCU=OPT_MCU_ESP32S3"
  "-DCFG_TUSB_OS=OPT_OS_FREERTOS"
)