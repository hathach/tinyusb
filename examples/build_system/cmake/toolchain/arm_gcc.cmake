if (NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER "arm-none-eabi-gcc")
endif ()

if (NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER "arm-none-eabi-g++")
endif ()

set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
set(CMAKE_SIZE "arm-none-eabi-size" CACHE FILEPATH "")
set(CMAKE_OBJCOPY "arm-none-eabi-objcopy" CACHE FILEPATH "")
set(CMAKE_OBJDUMP "arm-none-eabi-objdump" CACHE FILEPATH "")

include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)

get_property(IS_IN_TRY_COMPILE GLOBAL PROPERTY IN_TRY_COMPILE)
if (IS_IN_TRY_COMPILE)
  set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -nostdlib")
  set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -nostdlib")
  cmake_print_variables(CMAKE_C_LINK_FLAGS)
endif ()
