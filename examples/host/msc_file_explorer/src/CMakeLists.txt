# This file is for ESP-IDF only
set(FATFS_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../../lib/fatfs/source)
set(EMBEDDED_CLI_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../../lib/embedded-cli)

idf_component_register(
                SRCS "main.c" "msc_app.c"
                    ${FATFS_DIR}/ff.c
                    ${FATFS_DIR}/ffsystem.c
                    ${FATFS_DIR}/ffunicode.c
                INCLUDE_DIRS "." ${FATFS_DIR} ${EMBEDDED_CLI_DIR}
                REQUIRES boards tinyusb_src)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=format)
