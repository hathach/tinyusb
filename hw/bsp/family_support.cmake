if (NOT TARGET _family_support_marker)
    add_library(_family_support_marker INTERFACE)

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
                    # If mcu:xxx exists for this mcu then include
                    if (${_line} STREQUAL "mcu:${MCU}")
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
endif()