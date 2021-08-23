# Apply board specific content here
target_include_directories(${COMPONENT_LIB} PRIVATE .)

idf_build_get_property(idf_target IDF_TARGET)

message(STATUS "Apply ${BOARD}(${idf_target}) specific options for component: ${COMPONENT_TARGET}")

if(NOT ${idf_target} STREQUAL "esp32s3")
    message(FATAL_ERROR "Incorrect target for board ${BOARD}: (${idf_target}), try to clean the build first." )
endif()

set(IDF_TARGET "esp32s3" FORCE)

target_compile_options(${COMPONENT_TARGET} PUBLIC
  "-DCFG_TUSB_MCU=OPT_MCU_ESP32S3"
  "-DCFG_TUSB_OS=OPT_OS_FREERTOS"
)