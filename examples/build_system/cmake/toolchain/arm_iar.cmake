if (NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER "iccarm")
endif()

if (NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER "iccarm")
endif()

if (NOT DEFINED CMAKE_ASM_COMPILER)
  set(CMAKE_ASM_COMPILER "iasmarm")
endif()

find_program(CMAKE_SIZE size)
find_program(CMAKE_OBJCOPY ielftool)
find_program(CMAKE_OBJDUMP iefdumparm)

find_program(CMAKE_IAR_CSTAT icstat)
find_program(CMAKE_IAR_CHECKS ichecks)
find_program(CMAKE_IAR_REPORT ireport)

if (IAR_CSTAT)
cmake_minimum_required(VERSION 4.1)
set(CMAKE_C_ICSTAT ${CMAKE_IAR_CSTAT}
  --checks=${CMAKE_CURRENT_LIST_DIR}/cstat_sel_checks.txt
  --db=${CMAKE_BINARY_DIR}/cstat.db
  --sarif_dir=${CMAKE_BINARY_DIR}/cstat_sarif
  --exclude ${TOP}/hw/mcu --exclude ${TOP}/lib
  )
endif ()

include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)
