if (NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER "riscv-none-embed-gcc")
endif ()

if (NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER "riscv-none-embed-g++")
endif ()

set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
set(CMAKE_SIZE "riscv-none-embed-size" CACHE FILEPATH "")
set(CMAKE_OBJCOPY "riscv-none-embed-objcopy" CACHE FILEPATH "")
set(CMAKE_OBJDUMP "riscv-none-embed-objdump" CACHE FILEPATH "")

include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)

get_property(IS_IN_TRY_COMPILE GLOBAL PROPERTY IN_TRY_COMPILE)
if (IS_IN_TRY_COMPILE)
  set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -nostdlib")
  set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -nostdlib")
  cmake_print_variables(CMAKE_C_LINK_FLAGS)
endif ()
