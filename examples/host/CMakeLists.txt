cmake_minimum_required(VERSION 3.5)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../hw/bsp/family_support.cmake)

project(tinyusb_host_examples)
family_initialize_project(tinyusb_host_examples ${CMAKE_CURRENT_LIST_DIR})

# family_add_subdirectory will filter what to actually add based on selected FAMILY
family_add_subdirectory(bare_api)
family_add_subdirectory(cdc_msc_hid)
family_add_subdirectory(hid_controller)
family_add_subdirectory(msc_file_explorer)
