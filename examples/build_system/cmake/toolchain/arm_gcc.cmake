set(CMAKE_SYSTEM_NAME Generic)

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

set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)

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

list(APPEND TOOLCHAIN_EXE_LINKER_FLAGS
  -Wl,--print-memory-usage
  -Wl,--gc-sections
  -Wl,--cref
  )

include(${CMAKE_CURRENT_LIST_DIR}/set_flags.cmake)

# try_compile is cmake test compiling its own example,
# pass -nostdlib to skip stdlib linking
get_property(IS_IN_TRY_COMPILE GLOBAL PROPERTY IN_TRY_COMPILE)
if (IS_IN_TRY_COMPILE)
  set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -nostdlib")
  set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -nostdlib")
endif ()
