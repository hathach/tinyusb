## How to quick setup

1. python tools/get_deps.py -b stm32h743_weact
2. cd examples/device/cdc_msc
3. brew install --cask gcc-arm-embedded
3. cmake -DBOARD=stm32h743_weact -B build -DCMAKE_BUILD_TYPE=Debug
4. cmake --build build --parallel