###################################################################################
#                                                                                 #
# NAME: CMakeLists.txt                                                            #
#                                                                                 #
# AUTHOR: Mike Karlesky, Mark VanderVoord, Greg Williams.                         #
# WRITTEN BY: Michael Brockus.                                                    #
#                                                                                 #
# License: MIT                                                                    #
#                                                                                 #
###################################################################################
cmake_minimum_required(VERSION 3.0 FATAL_ERROR)


add_library(unity STATIC "unity.c")

install(TARGETS unity EXPORT unityConfig
    ARCHIVE  DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_INSTALL_LIBDIR}"
    LIBRARY  DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_INSTALL_LIBDIR}"
    RUNTIME  DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_LIBDIR}")


