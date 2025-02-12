# default Toolchain from https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack
if (NOT DEFINED CROSS_COMPILE)
  set(CROSS_COMPILE "riscv-none-elf-")
endif ()

if (NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER ${CROSS_COMPILE}gcc)
endif ()

if (NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER ${CROSS_COMPILE}gcc)
endif ()

if (NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER ${CROSS_COMPILE}g++)
endif ()

set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
find_program(CMAKE_SIZE ${CROSS_COMPILE}size)
find_program(CMAKE_OBJCOPY ${CROSS_COMPILE}objcopy)
find_program(CMAKE_OBJDUMP ${CROSS_COMPILE}objdump)

include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)

get_property(IS_IN_TRY_COMPILE GLOBAL PROPERTY IN_TRY_COMPILE)
if (IS_IN_TRY_COMPILE)
  set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -nostdlib")
  set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -nostdlib")
  cmake_print_variables(CMAKE_C_LINK_FLAGS)
endif ()
