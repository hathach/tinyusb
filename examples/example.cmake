target_compile_options(${PROJECT} PUBLIC
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

# GCC 10
if (CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 10.0)
  target_compile_options(${PROJECT} PUBLIC -Wconversion)
endif()

# GCC 8
if (CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 8.0)
  target_compile_options(${PROJECT} PUBLIC -Wcast-function-type -Wstrict-overflow)
endif()
