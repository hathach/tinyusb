if (NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER "msp430-elf-gcc")
endif ()

if (NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER "msp430-elf-g++")
endif ()

set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})

find_program(CMAKE_SIZE msp430-elf-size)
find_program(CMAKE_OBJCOPY msp430-elf-objcopy)
find_program(CMAKE_OBJDUMP msp430-elf-objdump)

include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)
