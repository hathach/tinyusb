![TinyUSB](docs/assets/logo.svg)

[![Build Status](https://github.com/hathach/tinyusb/workflows/Build/badge.svg)](https://github.com/hathach/tinyusb/actions) [![Documentation Status](https://readthedocs.org/projects/tinyusb/badge/?version=latest)](https://openinput.readthedocs.io/en/latest/?badge=latest) [![License](https://img.shields.io/badge/license-MIT-brightgreen.svg)](https://opensource.org/licenses/MIT)

TinyUSB is an open-source cross-platform USB Host/Device stack for embedded system, designed to be memory-safe with no dynamic allocation and thread-safe with all interrupt events are deferred then handled in the non-ISR task function.

Please head over to the online [documentation](www.tinyusb.org) for more info.

## Contributors

Special thanks to all the people who spent their precious time and effort to help this project so far. Check out the 
[CONTRIBUTORS](CONTRIBUTORS.rst) file for the list of all contributors and their awesome work for the stack.

## License

All TinyUSB sources in the `src` folder are licensed under MIT license, [Full license is here](LICENSE). However, each file can be individually licensed especially those in `lib` and `hw/mcu` folder. Please make sure you understand all the license term for files you use in your project.
