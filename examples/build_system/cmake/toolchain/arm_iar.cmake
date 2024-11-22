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

include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)
