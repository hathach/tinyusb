include(CMakePrintHelpers)
foreach(LANG IN ITEMS C CXX ASM)
    # join the toolchain flags into a single string
    list(APPEND TOOLCHAIN_${LANG}_FLAGS ${TOOLCHAIN_COMMON_FLAGS})
    list(JOIN TOOLCHAIN_${LANG}_FLAGS " " TOOLCHAIN_${LANG}_FLAGS)
    set(CMAKE_${LANG}_FLAGS_INIT "${TOOLCHAIN_${LANG}_FLAGS}")

    #cmake_print_variables(CMAKE_${LANG}_FLAGS_INIT)

    # optimization flags
    set(CMAKE_${LANG}_FLAGS_RELEASE_INIT "-Os")
    set(CMAKE_${LANG}_FLAGS_DEBUG_INIT "-O0")
endforeach()

# Linker
list(JOIN TOOLCHAIN_EXE_LINKER_FLAGS " " CMAKE_EXE_LINKER_FLAGS_INIT)

# try_compile is cmake test compiling its own example,
# pass -nostdlib to skip stdlib linking
get_property(IS_IN_TRY_COMPILE GLOBAL PROPERTY IN_TRY_COMPILE)
if(IS_IN_TRY_COMPILE)
    set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -nostdlib")
    set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -nostdlib")
endif()
