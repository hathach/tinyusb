###################################################################################
#                                                                                 #
# NAME: meson.build                                                               #
#                                                                                 #
# AUTHOR: Mike Karlesky, Mark VanderVoord, Greg Williams.                         #
# WRITTEN BY: Michael Brockus.                                                    #
#                                                                                 #
# License: MIT                                                                    #
#                                                                                 #
###################################################################################

cmock_dir = include_directories('.')

cmock_lib = static_library(meson.project_name(), 
    sources: ['cmock.c'],
    dependencies: [unity_dep],
    include_directories: cmock_dir)
