if (NOT DEFINED TOOLCHAIN OR TOOLCHAIN STREQUAL "gcc")
  set(TOOLCHAIN_COMMON_FLAGS
    -mcpu=rx610
    -misa=v1
    -mlittle-endian-data
    -fshort-enums
    )
  set(FREERTOS_PORT GCC_RX600 CACHE INTERNAL "")

else ()
  message(FATAL_ERROR "Toolchain ${TOOLCHAIN} is not supported for RX")
endif ()
