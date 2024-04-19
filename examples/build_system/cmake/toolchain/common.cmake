include(CMakePrintHelpers)

# ----------------------------------------------------------------------------
# Common
# ----------------------------------------------------------------------------
set(CMAKE_SYSTEM_NAME Generic)
set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)

# Look for includes and libraries only in the target system prefix.
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# pass TOOLCHAIN_CPU to
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CMAKE_SYSTEM_PROCESSOR)
include(${CMAKE_CURRENT_LIST_DIR}/../cpu/${CMAKE_SYSTEM_PROCESSOR}.cmake)

# ----------------------------------------------------------------------------
# Compile flags
# ----------------------------------------------------------------------------
if (TOOLCHAIN STREQUAL "gcc")
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

elseif (TOOLCHAIN STREQUAL "iar")
  #list(APPEND TOOLCHAIN_COMMON_FLAGS)
  list(APPEND TOOLCHAIN_EXE_LINKER_FLAGS
    --diag_suppress=Li065
    )

elseif (TOOLCHAIN STREQUAL "clang")
  list(APPEND TOOLCHAIN_COMMON_FLAGS
    -fdata-sections
    -ffunction-sections
    -fno-strict-aliasing
    )
  list(APPEND TOOLCHAIN_EXE_LINKER_FLAGS
    -Wl,--print-memory-usage
    -Wl,--gc-sections
    -Wl,--cref
    )
endif ()

# join the toolchain flags into a single string
list(JOIN TOOLCHAIN_COMMON_FLAGS " " TOOLCHAIN_COMMON_FLAGS)
foreach (LANG IN ITEMS C CXX ASM)
  set(CMAKE_${LANG}_FLAGS_INIT ${TOOLCHAIN_COMMON_FLAGS})
  # optimization flags for LOG, LOGGER ?
  #set(CMAKE_${LANG}_FLAGS_RELEASE_INIT "-Os")
  #set(CMAKE_${LANG}_FLAGS_DEBUG_INIT "-O0")
endforeach ()

# Linker
list(JOIN TOOLCHAIN_EXE_LINKER_FLAGS " " CMAKE_EXE_LINKER_FLAGS_INIT)
