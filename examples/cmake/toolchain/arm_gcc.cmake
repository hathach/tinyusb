set(CMAKE_SYSTEM_NAME Generic)

set(CMAKE_ASM_COMPILER "arm-none-eabi-gcc")
set(CMAKE_C_COMPILER "arm-none-eabi-gcc")
set(CMAKE_CXX_COMPILER "arm-none-eabi-g++")
set(GCC_ELF2BIN "arm-none-eabi-objcopy")
set_property(GLOBAL PROPERTY ELF2BIN ${GCC_ELF2BIN})

# Look for includes and libraries only in the target system prefix.
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# pass TOOLCHAIN_CPU to
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CMAKE_SYSTEM_PROCESSOR)

include(${CMAKE_CURRENT_LIST_DIR}/../cpu/${CMAKE_SYSTEM_PROCESSOR}.cmake)

# enable all possible warnings for building examples
list(APPEND TOOLCHAIN_COMMON_FLAGS
  -fdata-sections
  -ffunction-sections
  -fsingle-precision-constant
  -fno-strict-aliasing
  )

set(TOOLCHAIN_WARNING_FLAGS
  -Wall
  -Wextra
  -Werror
  -Wfatal-errors
  -Wdouble-promotion
  -Wstrict-prototypes
  -Wstrict-overflow
  -Werror-implicit-function-declaration
  -Wfloat-equal
  -Wundef
  -Wshadow
  -Wwrite-strings
  -Wsign-compare
  -Wmissing-format-attribute
  -Wunreachable-code
  -Wcast-align
  -Wcast-function-type
  -Wcast-qual
  -Wnull-dereference
  -Wuninitialized
  -Wunused
  -Wreturn-type
  -Wredundant-decls
  )

include(${CMAKE_CURRENT_LIST_DIR}/set_flags.cmake)
