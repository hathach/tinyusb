if( FREERTOS_PORT STREQUAL "GCC_RISC_V_GENERIC" )
    set( VALID_CHIP_EXTENSIONS
            "Pulpino_Vega_RV32M1RM"
            "RISCV_MTIME_CLINT_no_extensions"
            "RISCV_no_extensions"
            "RV32I_CLINT_no_extensions" )

    if( ( NOT FREERTOS_RISCV_EXTENSION ) OR ( NOT ( ${FREERTOS_RISCV_EXTENSION} IN_LIST VALID_CHIP_EXTENSIONS ) ) )
        message(FATAL_ERROR
                "FREERTOS_RISCV_EXTENSION \"${FREERTOS_RISCV_EXTENSION}\" is not set or unsupported.\n"
                "Please specify it from top-level CMake file (example):\n"
                "   set(FREERTOS_RISCV_EXTENSION RISCV_MTIME_CLINT_no_extensions CACHE STRING \"\")\n"
                " or from CMake command line option:\n"
                "   -DFREERTOS_RISCV_EXTENSION=RISCV_MTIME_CLINT_no_extensions\n"
                "\n"
                " Available extension options:\n"
                "   ${VALID_CHIP_EXTENSIONS} \n")
    endif()
endif()
