set(MCU_VARIANT MIMXRT1011)

target_sources(${PROJECT} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/evkmimxrt1010_flexspi_nor_config.c
  )

target_compile_definitions(${PROJECT} PUBLIC
  CPU_MIMXRT1011DAE5A
  CFG_EXAMPLE_VIDEO_READONLY
  )
