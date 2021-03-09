cmake_minimum_required(VERSION 3.5)

# Add example src and bsp directories
set(EXTRA_COMPONENT_DIRS "src" "${TOP}/hw/bsp/esp32sx/boards" "${TOP}/hw/bsp/esp32sx/components")  
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(SUPPORTED_TARGETS esp32s2 esp32s3)
