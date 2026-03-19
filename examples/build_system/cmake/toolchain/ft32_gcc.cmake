if (NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER "ft32-elf-gcc")
endif ()

if (NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER "ft32-elf-g++")
endif ()

set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})

find_program(CMAKE_SIZE ft32-elf-size)
find_program(CMAKE_OBJCOPY ft32-elf-objcopy)
find_program(CMAKE_OBJDUMP ft32-elf-objdump)

include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)

get_property(IS_IN_TRY_COMPILE GLOBAL PROPERTY IN_TRY_COMPILE)
if (IS_IN_TRY_COMPILE)
  set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -nostdlib")
  set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -nostdlib")
  cmake_print_variables(CMAKE_C_LINK_FLAGS)
endif ()
