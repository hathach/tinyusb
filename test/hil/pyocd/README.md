pyocd's project directory for the HIL pyocd flasher (`test/hil/hil_flash.py` `PYOCD_PROJECT_DIR`): relative roster
args such as `--pack` resolve from here. Keep any pyocd config file out of it: pyocd's probe discovery would load it.
