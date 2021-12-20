# Apply board specific content here
target_include_directories(${COMPONENT_LIB} PRIVATE .)

target_compile_options(${COMPONENT_TARGET} PUBLIC
  "-DCFG_TUSB_MCU=OPT_MCU_ESP32S3"
  "-DCFG_TUSB_OS=OPT_OS_FREERTOS"
)