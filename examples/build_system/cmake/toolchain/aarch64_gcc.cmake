if (NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER "aarch64-none-elf-gcc")
endif ()

if (NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER "aarch64-none-elf-g++")
endif ()

set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
set(CMAKE_SIZE "aarch64-none-elf-size" CACHE FILEPATH "")
set(CMAKE_OBJCOPY "aarch64-none-elf-objcopy" CACHE FILEPATH "")
set(CMAKE_OBJDUMP "aarch64-none-elf-objdump" CACHE FILEPATH "")

include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)

get_property(IS_IN_TRY_COMPILE GLOBAL PROPERTY IN_TRY_COMPILE)
if (IS_IN_TRY_COMPILE)
  set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -nostdlib")
  set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -nostdlib")
  cmake_print_variables(CMAKE_C_LINK_FLAGS)
endif ()
