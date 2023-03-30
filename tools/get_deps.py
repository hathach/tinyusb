import sys
import subprocess
from pathlib import Path
from multiprocessing import Pool

# Mandatory Dependencies that is always fetched
# path, url, commit (Alphabet sorted by path)
deps_mandatory = {
    'lib/FreeRTOS-Kernel'                      : ['def7d2df2b0506d3d249334974f51e427c17a41c', 'https://github.com/FreeRTOS/FreeRTOS-Kernel.git'               ],
    'lib/lwip'                                 : ['159e31b689577dbf69cf0683bbaffbd71fa5ee10', 'https://github.com/lwip-tcpip/lwip.git'                        ],
    'tools/uf2'                                : ['19615407727073e36d81bf239c52108ba92e7660', 'https://github.com/microsoft/uf2.git'                          ],
}

# Optional Dependencies per MCU
# path, url, commit (Alphabet sorted by path)
deps_optional = {
    'hw/mcu/allwinner'                         : ['8e5e89e8e132c0fd90e72d5422e5d3d68232b756', 'https://github.com/hathach/allwinner_driver.git'               ],
    'hw/mcu/bridgetek/ft9xx/ft90x-sdk'         : ['91060164afe239fcb394122e8bf9eb24d3194eb1', 'https://github.com/BRTSG-FOSS/ft90x-sdk.git'                   ],
    'hw/mcu/broadcom'                          : ['08370086080759ed54ac1136d62d2ad24c6fa267', 'https://github.com/adafruit/broadcom-peripherals.git'          ],
    'hw/mcu/gd/nuclei-sdk'                     : ['7eb7bfa9ea4fbeacfafe1d5f77d5a0e6ed3922e7', 'https://github.com/Nuclei-Software/nuclei-sdk.git'             ],
    'hw/mcu/infineon/mtb-xmclib-cat3'          : ['daf5500d03cba23e68c2f241c30af79cd9d63880', 'https://github.com/Infineon/mtb-xmclib-cat3.git'               ],
    'hw/mcu/microchip'                         : ['9e8b37e307d8404033bb881623a113931e1edf27', 'https://github.com/hathach/microchip_driver.git'               ],
    'hw/mcu/mindmotion/mm32sdk'                : ['0b79559eb411149d36e073c1635c620e576308d4', 'https://github.com/hathach/mm32sdk.git'                        ],
    'hw/mcu/nordic/nrfx'                       : ['281cc2e178fd9a470d844b3afdea9eb322a0b0e8', 'https://github.com/NordicSemiconductor/nrfx.git'               ],
    'hw/mcu/nuvoton'                           : ['2204191ec76283371419fbcec207da02e1bc22fa', 'https://github.com/majbthrd/nuc_driver.git'                    ],
    'hw/mcu/nxp/lpcopen'                       : ['43c45c85405a5dd114fff0ea95cca62837740c13', 'https://github.com/hathach/nxp_lpcopen.git'                    ],
    'hw/mcu/nxp/mcux-sdk'                      : ['ae2ab01d9d70ad00cd0e935c2552bd5f0e5c0294', 'https://github.com/NXPmicro/mcux-sdk.git'                      ],
    'hw/mcu/nxp/nxp_sdk'                       : ['845c8fc49b6fb660f06a5c45225494eacb06f00c', 'https://github.com/hathach/nxp_sdk.git'                        ],
    'hw/mcu/raspberry_pi/Pico-PIO-USB'         : ['c3715ce94b6f6391856de56081d4d9b3e98fa93d', 'https://github.com/sekigon-gonnoc/Pico-PIO-USB.git'            ],
    'hw/mcu/renesas/fsp'                       : ['8dc14709f2a6518b43f71efad70d900b7718d9f1', 'https://github.com/renesas/fsp.git'                            ],
    'hw/mcu/renesas/rx'                        : ['706b4e0cf485605c32351e2f90f5698267996023', 'https://github.com/kkitayam/rx_device.git'                     ],
    'hw/mcu/silabs/cmsis-dfp-efm32gg12b'       : ['f1c31b7887669cb230b3ea63f9b56769078960bc', 'https://github.com/cmsis-packs/cmsis-dfp-efm32gg12b.git'       ],
    'hw/mcu/sony/cxd56/spresense-exported-sdk' : ['2ec2a1538362696118dc3fdf56f33dacaf8f4067', 'https://github.com/sonydevworld/spresense-exported-sdk.git'    ],
    'hw/mcu/st/cmsis_device_f0'                : ['2fc25ee22264bc27034358be0bd400b893ef837e', 'https://github.com/STMicroelectronics/cmsis_device_f0.git'     ],
    'hw/mcu/st/cmsis_device_f1'                : ['6601104a6397299b7304fd5bcd9a491f56cb23a6', 'https://github.com/STMicroelectronics/cmsis_device_f1.git'     ],
    'hw/mcu/st/cmsis_device_f2'                : ['182fcb3681ce116816feb41b7764f1b019ce796f', 'https://github.com/STMicroelectronics/cmsis_device_f2.git'     ],
    'hw/mcu/st/cmsis_device_f3'                : ['5e4ee5ed7a7b6c85176bb70a9fd3c72d6eb99f1b', 'https://github.com/STMicroelectronics/cmsis_device_f3.git'     ],
    'hw/mcu/st/cmsis_device_f4'                : ['2615e866fa48fe1ff1af9e31c348813f2b19e7ec', 'https://github.com/STMicroelectronics/cmsis_device_f4.git'     ],
    'hw/mcu/st/cmsis_device_f7'                : ['fc676ef1ad177eb874eaa06444d3d75395fc51f4', 'https://github.com/STMicroelectronics/cmsis_device_f7.git'     ],
    'hw/mcu/st/cmsis_device_g0'                : ['08258b28ee95f50cb9624d152a1cbf084be1f9a5', 'https://github.com/STMicroelectronics/cmsis_device_g0.git'     ],
    'hw/mcu/st/cmsis_device_g4'                : ['ce822adb1dc552b3aedd13621edbc7fdae124878', 'https://github.com/STMicroelectronics/cmsis_device_g4.git'     ],
    'hw/mcu/st/cmsis_device_h7'                : ['60dc2c913203dc8629dc233d4384dcc41c91e77f', 'https://github.com/STMicroelectronics/cmsis_device_h7.git'     ],
    'hw/mcu/st/cmsis_device_l0'                : ['06748ca1f93827befdb8b794402320d94d02004f', 'https://github.com/STMicroelectronics/cmsis_device_l0.git'     ],
    'hw/mcu/st/cmsis_device_l1'                : ['7f16ec0a1c4c063f84160b4cc6bf88ad554a823e', 'https://github.com/STMicroelectronics/cmsis_device_l1.git'     ],
    'hw/mcu/st/cmsis_device_l4'                : ['6ca7312fa6a5a460b5a5a63d66da527fdd8359a6', 'https://github.com/STMicroelectronics/cmsis_device_l4.git'     ],
    'hw/mcu/st/cmsis_device_l5'                : ['d922865fc0326a102c26211c44b8e42f52c1e53d', 'https://github.com/STMicroelectronics/cmsis_device_l5.git'     ],
    'hw/mcu/st/cmsis_device_u5'                : ['bc00f3c9d8a4e25220f84c26d414902cc6bdf566', 'https://github.com/STMicroelectronics/cmsis_device_u5.git'     ],
    'hw/mcu/st/cmsis_device_wb'                : ['9c5d1920dd9fabbe2548e10561d63db829bb744f', 'https://github.com/STMicroelectronics/cmsis_device_wb.git'     ],
    'hw/mcu/st/stm32f0xx_hal_driver'           : ['0e95cd88657030f640a11e690a8a5186c7712ea5', 'https://github.com/STMicroelectronics/stm32f0xx_hal_driver.git'],
    'hw/mcu/st/stm32f1xx_hal_driver'           : ['1dd9d3662fb7eb2a7f7d3bc0a4c1dc7537915a29', 'https://github.com/STMicroelectronics/stm32f1xx_hal_driver.git'],
    'hw/mcu/st/stm32f2xx_hal_driver'           : ['c75ace9b908a9aca631193ebf2466963b8ea33d0', 'https://github.com/STMicroelectronics/stm32f2xx_hal_driver.git'],
    'hw/mcu/st/stm32f3xx_hal_driver'           : ['1761b6207318ede021706e75aae78f452d72b6fa', 'https://github.com/STMicroelectronics/stm32f3xx_hal_driver.git'],
    'hw/mcu/st/stm32f4xx_hal_driver'           : ['04e99fbdabd00ab8f370f377c66b0a4570365b58', 'https://github.com/STMicroelectronics/stm32f4xx_hal_driver.git'],
    'hw/mcu/st/stm32f7xx_hal_driver'           : ['f7ffdf6bf72110e58b42c632b0a051df5997e4ee', 'https://github.com/STMicroelectronics/stm32f7xx_hal_driver.git'],
    'hw/mcu/st/stm32g0xx_hal_driver'           : ['5b53e6cee664a82b16c86491aa0060e2110c00cb', 'https://github.com/STMicroelectronics/stm32g0xx_hal_driver.git'],
    'hw/mcu/st/stm32g4xx_hal_driver'           : ['8b4518417706d42eef5c14e56a650005abf478a8', 'https://github.com/STMicroelectronics/stm32g4xx_hal_driver.git'],
    'hw/mcu/st/stm32h7xx_hal_driver'           : ['d8461b980b59b1625207d8c4f2ce0a9c2a7a3b04', 'https://github.com/STMicroelectronics/stm32h7xx_hal_driver.git'],
    'hw/mcu/st/stm32l0xx_hal_driver'           : ['fbdacaf6f8c82a4e1eb9bd74ba650b491e97e17b', 'https://github.com/STMicroelectronics/stm32l0xx_hal_driver.git'],
    'hw/mcu/st/stm32l1xx_hal_driver'           : ['44efc446fa69ed8344e7fd966e68ed11043b35d9', 'https://github.com/STMicroelectronics/stm32l1xx_hal_driver.git'],
    'hw/mcu/st/stm32l4xx_hal_driver'           : ['aee3d5bf283ae5df87532b781bdd01b7caf256fc', 'https://github.com/STMicroelectronics/stm32l4xx_hal_driver.git'],
    'hw/mcu/st/stm32l5xx_hal_driver'           : ['675c32a75df37f39d50d61f51cb0dcf53f07e1cb', 'https://github.com/STMicroelectronics/stm32l5xx_hal_driver.git'],
    'hw/mcu/st/stm32u5xx_hal_driver'           : ['2e1d4cdb386e33391cb261dfff4fefa92e4aa35a', 'https://github.com/STMicroelectronics/stm32u5xx_hal_driver.git'],
    'hw/mcu/st/stm32wbxx_hal_driver'           : ['2c5f06638be516c1b772f768456ba637f077bac8', 'https://github.com/STMicroelectronics/stm32wbxx_hal_driver.git'],
    'hw/mcu/ti'                                : ['143ed6cc20a7615d042b03b21e070197d473e6e5', 'https://github.com/hathach/ti_driver.git'                      ],
    'hw/mcu/wch/ch32v307'                      : ['17761f5cf9dbbf2dcf665b7c04934188add20082', 'https://github.com/openwch/ch32v307.git'                       ],
    'lib/CMSIS_5'                              : ['20285262657d1b482d132d20d755c8c330d55c1f', 'https://github.com/ARM-software/CMSIS_5.git'                   ],
    'lib/sct_neopixel'                         : ['e73e04ca63495672d955f9268e003cffe168fcd8', 'https://github.com/gsteiert/sct_neopixel.git'                  ],
}

# combined 2 deps
deps_all = {**deps_mandatory, **deps_optional}

# TOP is tinyusb root dir
TOP = Path(__file__).parent.parent.resolve()


def get_a_dep(d):
    if d not in deps_all.keys():
        print('{} is not found in dependency list')
        return 1
    commit = deps_all[d][0]
    url = deps_all[d][1]
    print('cloning {} with {}'.format(d, url))

    p = Path(TOP / d)
    git_cmd = "git -C {}".format(p)

    # Init git deps if not existed
    if not p.exists():
        p.mkdir(parents=True)
        subprocess.run("{} init".format(git_cmd), shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        subprocess.run("{} remote add origin {}".format(git_cmd, url), shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    # Check if commit is already fetched
    result = subprocess.run("{} rev-parse HEAD".format(git_cmd, commit), shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    head = result.stdout.decode("utf-8").splitlines()[0]

    if commit != head:
        subprocess.run("{} reset --hard".format(git_cmd, commit), shell=True)
        subprocess.run("{} fetch --depth 1 origin {}".format(git_cmd, commit), shell=True)
        subprocess.run("{} checkout FETCH_HEAD".format(git_cmd), shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    return 0


if __name__ == "__main__":
    status = 0
    deps = list(deps_mandatory.keys())
    # get all if executed with all as argument
    if len(sys.argv) == 2 and sys.argv[1] == 'all':
        deps += deps_optional
    else:
        deps += sys.argv[1:]

    with Pool() as pool:
        status = sum(pool.map(get_a_dep, deps))
    sys.exit(status)
