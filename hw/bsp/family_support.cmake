if (TARGET _family_support_marker)
    return()
endif ()

add_library(_family_support_marker INTERFACE)

include(CMakePrintHelpers)

# Default to gcc
if(NOT DEFINED TOOLCHAIN)
    set(TOOLCHAIN gcc)
endif()

if (NOT FAMILY)
    message(FATAL_ERROR "You must set a FAMILY variable for the build (e.g. rp2040, eps32s2, esp32s3). You can do this via -DFAMILY=xxx on the cmake command line")
endif()

if (NOT EXISTS ${CMAKE_CURRENT_LIST_DIR}/${FAMILY}/family.cmake)
    message(FATAL_ERROR "Family '${FAMILY}' is not known/supported")
endif()

function(family_filter RESULT DIR)
    get_filename_component(DIR ${DIR} ABSOLUTE BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

    if (EXISTS "${DIR}/only.txt")
        file(READ "${DIR}/only.txt" ONLYS)
        # Replace newlines with semicolon so that it is treated as a list by CMake
        string(REPLACE "\n" ";" ONLYS_LINES ${ONLYS})
        # For each mcu
        foreach(MCU IN LISTS FAMILY_MCUS)
            # For each line in only.txt
            foreach(_line ${ONLYS_LINES})
                # If mcu:xxx exists for this mcu or board:xxx then include
                if (${_line} STREQUAL "mcu:${MCU}" OR ${_line} STREQUAL "board:${BOARD}")
                    set(${RESULT} 1 PARENT_SCOPE)
                    return()
                endif()
            endforeach()
        endforeach()

        # Didn't find it in only file so don't build
        set(${RESULT} 0 PARENT_SCOPE)

    elseif (EXISTS "${DIR}/skip.txt")
        file(READ "${DIR}/skip.txt" SKIPS)
        # Replace newlines with semicolon so that it is treated as a list by CMake
        string(REPLACE "\n" ";" SKIPS_LINES ${SKIPS})
        # For each mcu
        foreach(MCU IN LISTS FAMILY_MCUS)
            # For each line in only.txt
            foreach(_line ${SKIPS_LINES})
                # If mcu:xxx exists for this mcu then skip
                if (${_line} STREQUAL "mcu:${MCU}")
                    set(${RESULT} 0 PARENT_SCOPE)
                    return()
                endif()
            endforeach()
        endforeach()

        # Didn't find in skip file so build
        set(${RESULT} 1 PARENT_SCOPE)

    else()

        # Didn't find skip or only file so build
        set(${RESULT} 1 PARENT_SCOPE)

    endif()

endfunction()

function(family_add_subdirectory DIR)
    family_filter(SHOULD_ADD "${DIR}")
    if (SHOULD_ADD)
        add_subdirectory(${DIR})
    endif()
endfunction()

function(family_get_project_name OUTPUT_NAME DIR)
    get_filename_component(SHORT_NAME ${DIR} NAME)
    set(${OUTPUT_NAME} ${TINYUSB_FAMILY_PROJECT_NAME_PREFIX}${SHORT_NAME} PARENT_SCOPE)
endfunction()

function(family_initialize_project PROJECT DIR)
    family_filter(ALLOWED "${DIR}")
    if (NOT ALLOWED)
        get_filename_component(SHORT_NAME ${DIR} NAME)
        message(FATAL_ERROR "${SHORT_NAME} is not supported on FAMILY=${FAMILY}")
    endif()
endfunction()

function(family_add_default_example_warnings TARGET)
    target_compile_options(${TARGET} PUBLIC
            -Wall
            -Wextra
            -Werror
            -Wfatal-errors
            -Wdouble-promotion
            -Wfloat-equal
            -Wshadow
            -Wwrite-strings
            -Wsign-compare
            -Wmissing-format-attribute
            -Wunreachable-code
            -Wcast-align
            -Wcast-qual
            -Wnull-dereference
            -Wuninitialized
            -Wunused
            -Wredundant-decls
            #-Wstrict-prototypes
            #-Werror-implicit-function-declaration
            #-Wundef
            )

    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
        if (CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0)
            target_link_options(${TARGET} PUBLIC "LINKER:--no-warn-rwx-segments")
        endif()

        # GCC 10
        if (CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 10.0)
            target_compile_options(${TARGET} PUBLIC -Wconversion)
        endif()

        # GCC 8
        if (CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 8.0)
            target_compile_options(${TARGET} PUBLIC -Wcast-function-type -Wstrict-overflow)
        endif()

        # GCC 6
        if (CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 6.0)
            target_compile_options(${TARGET} PUBLIC -Wno-strict-aliasing)
        endif()
    endif()
endfunction()


#  add_custom_command(TARGET ${TARGET} POST_BUILD
#    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${TARGET}> ${TARGET}.hex
#    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET}> ${TARGET}.bin
#    COMMENT "Creating ${TARGET}.hex and ${TARGET}.bin"
#    )

# Add flash jlink target
function(family_flash_jlink TARGET)
    if (NOT DEFINED JLINKEXE)
        set(JLINKEXE JLinkExe)
    endif ()

    file(GENERATE
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.jlink
      CONTENT "halt
loadfile $<TARGET_FILE:${TARGET}>
r
go
exit"
      )

    add_custom_target(${TARGET}-jlink
      DEPENDS ${TARGET}
      COMMAND ${JLINKEXE} -device ${JLINK_DEVICE} -if swd -JTAGConf -1,-1 -speed auto -CommandFile ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.jlink
      )
endfunction()

# Add flash pycod target
function(family_flash_pyocd TARGET)
    if (NOT DEFINED PYOC)
        set(PYOCD pyocd)
    endif ()

    add_custom_target(${TARGET}-pyocd
      DEPENDS ${TARGET}
      COMMAND ${PYOCD} flash -t ${PYOCD_TARGET} $<TARGET_FILE:${TARGET}>
      )
endfunction()

# Add flash using NXP's LinkServer (redserver)
# https://www.nxp.com/design/software/development-software/mcuxpresso-software-and-tools-/linkserver-for-microcontrollers:LINKERSERVER
function(family_flash_nxplink TARGET)
    if (NOT DEFINED LINKSERVER)
        set(LINKSERVER LinkServer)
    endif ()

    # LinkServer has a bug that can only execute with full path otherwise it throws:
    # realpath error: No such file or directory
    execute_process(COMMAND which ${LINKSERVER} OUTPUT_VARIABLE LINKSERVER_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)

    add_custom_target(${TARGET}-nxplink
      DEPENDS ${TARGET}
      COMMAND ${LINKSERVER_PATH} flash ${NXPLINK_DEVICE} load $<TARGET_FILE:${TARGET}>
      )
endfunction()

# configure an executable target to link to tinyusb in device mode, and add the board implementation
function(family_configure_device_example TARGET)
    # default implementation is empty, the function should be redefined in the FAMILY/family.cmake
endfunction()

# configure an executable target to link to tinyusb in host mode, and add the board implementation
function(family_configure_host_example TARGET)
    # default implementation is empty, the function should be redefined in the FAMILY/family.cmake
endfunction()

include(${CMAKE_CURRENT_LIST_DIR}/${FAMILY}/family.cmake)

if (NOT FAMILY_MCUS)
    set(FAMILY_MCUS ${FAMILY})
endif()

# save it in case of re-inclusion
set(FAMILY_MCUS ${FAMILY_MCUS} CACHE INTERNAL "")
