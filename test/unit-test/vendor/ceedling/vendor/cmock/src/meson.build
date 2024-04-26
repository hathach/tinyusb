#
# build script written by : Michael Brockus.
# github repo author: Mike Karlesky, Mark VanderVoord, Greg Williams.
#
# license: MIT
#
cmock_dir = include_directories('.')

cmock_lib = static_library(meson.project_name(),
    files('cmock.c'),
    dependencies: [unity_dep],
    include_directories: cmock_dir)
