if (NOT TARGET _family_support_marker)
    add_library(_family_support_marker INTERFACE)

    if (NOT FAMILY)
        message(FATAL_ERROR "You must set a FAMILY variable for the build (e.g. rp2040, eps32s2, esp32s3). You can do this via -DFAMILY=xxx on the camke command line")
    endif()

    if (NOT EXISTS ${CMAKE_CURRENT_LIST_DIR}/${FAMILY}/family.cmake)
        message(FATAL_ERROR "Family '${FAMILY}' is not known/supported")
    endif()

    function(family_filter RESULT DIR)
        get_filename_component(DIR ${DIR} ABSOLUTE BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
        file(GLOB ONLYS "${DIR}/.only.MCU_*")
        if (ONLYS)
            foreach(MCU IN LISTS FAMILY_MCUS)
                if (EXISTS ${DIR}/.only.MCU_${MCU})
                    set(${RESULT} 1 PARENT_SCOPE)
                    return()
                endif()
            endforeach()
        else()
            foreach(MCU IN LISTS FAMILY_MCUS)
                if (EXISTS ${DIR}/.skip.MCU_${MCU})
                    set(${RESULT} 0 PARENT_SCOPE)
                    return()
                endif()
            endforeach()
        endif()
        set(${RESULT} 1 PARENT_SCOPE)
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

    # configure an executable target to link to tinyusb in device mode, and add the board implementation
    function(family_configure_device_example TARGET)
        # default implentation is empty, the function should be redefined in the FAMILY/family.cmake
    endfunction()

    # configure an executable target to link to tinyusb in host mode, and add the board implementation
    function(family_configure_host_example TARGET)
        # default implentation is empty, the function should be redefined in the FAMILY/family.cmake
    endfunction()

    include(${CMAKE_CURRENT_LIST_DIR}/${FAMILY}/family.cmake)

    if (NOT FAMILY_MCUS)
        set(FAMILY_MCUS ${FAMILY})
    endif()

    # save it in case of re-inclusion
    set(FAMILY_MCUS ${FAMILY_MCUS} CACHE INTERNAL "")
endif()