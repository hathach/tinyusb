include(CMakePrintHelpers)

# ----------------------------------------------------------------------------
# Common
# ----------------------------------------------------------------------------
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ${CMAKE_SYSTEM_CPU})
set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)

# Look for includes and libraries only in the target system prefix.
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# pass TOOLCHAIN_CPU to
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CMAKE_SYSTEM_PROCESSOR CMAKE_SYSTEM_CPU)
include(${CMAKE_CURRENT_LIST_DIR}/../cpu/${CMAKE_SYSTEM_CPU}.cmake)

# ----------------------------------------------------------------------------
# Compile flags
# ----------------------------------------------------------------------------
set(TOOLCHAIN_C_FLAGS)
set(TOOLCHAIN_ASM_FLAGS)
set(TOOLCHAIN_EXE_LINKER_FLAGS)

if (TOOLCHAIN STREQUAL "gcc" OR TOOLCHAIN STREQUAL "clang")
  list(APPEND TOOLCHAIN_COMMON_FLAGS
    -fdata-sections
    -ffunction-sections
#    -fsingle-precision-constant # not supported by clang
    -fno-strict-aliasing
    -g # include debug info for bloaty
    )
  set(TOOLCHAIN_EXE_LINKER_FLAGS "-Wl,--print-memory-usage -Wl,--gc-sections -Wl,--cref")

  if (TOOLCHAIN STREQUAL clang)
    set(TOOLCHAIN_ASM_FLAGS "-x assembler-with-cpp")
  endif ()
elseif (TOOLCHAIN STREQUAL "iar")
  set(TOOLCHAIN_C_FLAGS --debug)
  set(TOOLCHAIN_EXE_LINKER_FLAGS --diag_suppress=Li065)
endif ()

# join the toolchain flags into a single string
list(JOIN TOOLCHAIN_COMMON_FLAGS " " TOOLCHAIN_COMMON_FLAGS)

set(CMAKE_C_FLAGS_INIT "${TOOLCHAIN_COMMON_FLAGS} ${TOOLCHAIN_C_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${TOOLCHAIN_COMMON_FLAGS} ${TOOLCHAIN_C_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${TOOLCHAIN_COMMON_FLAGS} ${TOOLCHAIN_ASM_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT ${TOOLCHAIN_EXE_LINKER_FLAGS})
