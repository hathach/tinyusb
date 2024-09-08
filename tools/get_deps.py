import argparse
import sys
import subprocess
from pathlib import Path
from multiprocessing import Pool

# Mandatory Dependencies that is always fetched
# path, url, commit, family (Alphabet sorted by path)
deps_mandatory = {
    'lib/FreeRTOS-Kernel': ['https://github.com/FreeRTOS/FreeRTOS-Kernel.git',
                            'cc0e0707c0c748713485b870bb980852b210877f',
                            'all'],
    'lib/lwip': ['https://github.com/lwip-tcpip/lwip.git',
                 '159e31b689577dbf69cf0683bbaffbd71fa5ee10',
                 'all'],
    'tools/uf2': ['https://github.com/microsoft/uf2.git',
                  'c594542b2faa01cc33a2b97c9fbebc38549df80a',
                  'all'],
}

# Optional Dependencies per MCU
# path, url, commit, family (Alphabet sorted by path)
deps_optional = {
    'hw/mcu/allwinner': ['https://github.com/hathach/allwinner_driver.git',
                         '8e5e89e8e132c0fd90e72d5422e5d3d68232b756',
                         'fc100s'],
    'hw/mcu/analog/max32' : ['https://github.com/analogdevicesinc/msdk.git',
                             'b20b398d3e5e2007594e54a74ba3d2a2e50ddd75',
                             'max32650 max32666 max32690 max78002'],
    'hw/mcu/bridgetek/ft9xx/ft90x-sdk': ['https://github.com/BRTSG-FOSS/ft90x-sdk.git',
                                         '91060164afe239fcb394122e8bf9eb24d3194eb1',
                                         'brtmm90x'],
    'hw/mcu/broadcom': ['https://github.com/adafruit/broadcom-peripherals.git',
                        '08370086080759ed54ac1136d62d2ad24c6fa267',
                        'broadcom_32bit broadcom_64bit'],
    'hw/mcu/gd/nuclei-sdk': ['https://github.com/Nuclei-Software/nuclei-sdk.git',
                             '7eb7bfa9ea4fbeacfafe1d5f77d5a0e6ed3922e7',
                             'gd32vf103'],
    'hw/mcu/infineon/mtb-xmclib-cat3': ['https://github.com/Infineon/mtb-xmclib-cat3.git',
                                        'daf5500d03cba23e68c2f241c30af79cd9d63880',
                                        'xmc4000'],
    'hw/mcu/microchip': ['https://github.com/hathach/microchip_driver.git',
                         '9e8b37e307d8404033bb881623a113931e1edf27',
                         'sam3x samd11 samd21 samd51 samd5x_e5x same5x same7x saml2x samg'],
    'hw/mcu/mindmotion/mm32sdk': ['https://github.com/hathach/mm32sdk.git',
                                  'b93e856211060ae825216c6a1d6aa347ec758843',
                                  'mm32'],
    'hw/mcu/nordic/nrfx': ['https://github.com/NordicSemiconductor/nrfx.git',
                           '7c47cc0a56ce44658e6da2458e86cd8783ccc4a2',
                           'nrf'],
    'hw/mcu/nuvoton': ['https://github.com/majbthrd/nuc_driver.git',
                       '2204191ec76283371419fbcec207da02e1bc22fa',
                       'nuc'],
    'hw/mcu/nxp/lpcopen': ['https://github.com/hathach/nxp_lpcopen.git',
                           'b41cf930e65c734d8ec6de04f1d57d46787c76ae',
                           'lpc11 lpc13 lpc15 lpc17 lpc18 lpc40 lpc43'],
    'hw/mcu/nxp/mcux-sdk': ['https://github.com/hathach/mcux-sdk.git',
                            '144f1eb7ea8c06512e12f12b27383601c0272410',
                            'kinetis_k kinetis_k32l2 kinetis_kl lpc51 lpc54 lpc55 mcx imxrt'],
    'hw/mcu/raspberry_pi/Pico-PIO-USB': ['https://github.com/sekigon-gonnoc/Pico-PIO-USB.git',
                                         'fe9133fc513b82cc3dc62c67cb51f2339cf29ef7',
                                         'rp2040'],
    'hw/mcu/renesas/fsp': ['https://github.com/renesas/fsp.git',
                           'd52e5a6a59b7c638da860c2bb309b6e78e752ff8',
                           'ra'],
    'hw/mcu/renesas/rx': ['https://github.com/kkitayam/rx_device.git',
                          '706b4e0cf485605c32351e2f90f5698267996023',
                          'rx'],
    'hw/mcu/silabs/cmsis-dfp-efm32gg12b': ['https://github.com/cmsis-packs/cmsis-dfp-efm32gg12b.git',
                                           'f1c31b7887669cb230b3ea63f9b56769078960bc',
                                           'efm32'],
    'hw/mcu/sony/cxd56/spresense-exported-sdk': ['https://github.com/sonydevworld/spresense-exported-sdk.git',
                                                 '2ec2a1538362696118dc3fdf56f33dacaf8f4067',
                                                 'spresense'],
    'hw/mcu/st/cmsis_device_f0': ['https://github.com/STMicroelectronics/cmsis_device_f0.git',
                                  '2fc25ee22264bc27034358be0bd400b893ef837e',
                                  'stm32f0'],
    'hw/mcu/st/cmsis_device_f1': ['https://github.com/STMicroelectronics/cmsis_device_f1.git',
                                  '6601104a6397299b7304fd5bcd9a491f56cb23a6',
                                  'stm32f1'],
    'hw/mcu/st/cmsis_device_f2': ['https://github.com/STMicroelectronics/cmsis_device_f2.git',
                                  '182fcb3681ce116816feb41b7764f1b019ce796f',
                                  'stm32f2'],
    'hw/mcu/st/cmsis_device_f3': ['https://github.com/STMicroelectronics/cmsis_device_f3.git',
                                  '5e4ee5ed7a7b6c85176bb70a9fd3c72d6eb99f1b',
                                  'stm32f3'],
    'hw/mcu/st/cmsis_device_f4': ['https://github.com/STMicroelectronics/cmsis_device_f4.git',
                                  '2615e866fa48fe1ff1af9e31c348813f2b19e7ec',
                                  'stm32f4'],
    'hw/mcu/st/cmsis_device_f7': ['https://github.com/STMicroelectronics/cmsis_device_f7.git',
                                  '25b0463439303b7a38f0d27b161f7d2f3c096e79',
                                  'stm32f7'],
    'hw/mcu/st/cmsis_device_g0': ['https://github.com/STMicroelectronics/cmsis_device_g0.git',
                                  '3a23e1224417f3f2d00300ecd620495e363f2094',
                                  'stm32g0'],
    'hw/mcu/st/cmsis_device_g4': ['https://github.com/STMicroelectronics/cmsis_device_g4.git',
                                  'ce822adb1dc552b3aedd13621edbc7fdae124878',
                                  'stm32g4'],
    'hw/mcu/st/cmsis_device_h7': ['https://github.com/STMicroelectronics/cmsis_device_h7.git',
                                  '60dc2c913203dc8629dc233d4384dcc41c91e77f',
                                  'stm32h7'],
    'hw/mcu/st/cmsis_device_h5': ['https://github.com/STMicroelectronics/cmsis_device_h5.git',
                                  'cd2d1d579743de57b88ccaf61a968b9c05848ffc',
                                  'stm32h5'],
    'hw/mcu/st/cmsis_device_l0': ['https://github.com/STMicroelectronics/cmsis_device_l0.git',
                                  '69cd5999fd40ae6e546d4905b21635c6ca1bcb92',
                                  'stm32l0'],
    'hw/mcu/st/cmsis_device_l1': ['https://github.com/STMicroelectronics/cmsis_device_l1.git',
                                  '7f16ec0a1c4c063f84160b4cc6bf88ad554a823e',
                                  'stm32l1'],
    'hw/mcu/st/cmsis_device_l4': ['https://github.com/STMicroelectronics/cmsis_device_l4.git',
                                  '6ca7312fa6a5a460b5a5a63d66da527fdd8359a6',
                                  'stm32l4'],
    'hw/mcu/st/cmsis_device_l5': ['https://github.com/STMicroelectronics/cmsis_device_l5.git',
                                  'd922865fc0326a102c26211c44b8e42f52c1e53d',
                                  'stm32l5'],
    'hw/mcu/st/cmsis_device_u5': ['https://github.com/STMicroelectronics/cmsis_device_u5.git',
                                  '5ad9797c54ec3e55eff770fc9b3cd4a1aefc1309',
                                  'stm32u5'],
    'hw/mcu/st/cmsis_device_wb': ['https://github.com/STMicroelectronics/cmsis_device_wb.git',
                                  '9c5d1920dd9fabbe2548e10561d63db829bb744f',
                                  'stm32wb'],
    'hw/mcu/st/stm32f0xx_hal_driver': ['https://github.com/STMicroelectronics/stm32f0xx_hal_driver.git',
                                       '0e95cd88657030f640a11e690a8a5186c7712ea5',
                                       'stm32f0'],
    'hw/mcu/st/stm32f1xx_hal_driver': ['https://github.com/STMicroelectronics/stm32f1xx_hal_driver.git',
                                       '1dd9d3662fb7eb2a7f7d3bc0a4c1dc7537915a29',
                                       'stm32f1'],
    'hw/mcu/st/stm32f2xx_hal_driver': ['https://github.com/STMicroelectronics/stm32f2xx_hal_driver.git',
                                       'c75ace9b908a9aca631193ebf2466963b8ea33d0',
                                       'stm32f2'],
    'hw/mcu/st/stm32f3xx_hal_driver': ['https://github.com/STMicroelectronics/stm32f3xx_hal_driver.git',
                                       '1761b6207318ede021706e75aae78f452d72b6fa',
                                       'stm32f3'],
    'hw/mcu/st/stm32f4xx_hal_driver': ['https://github.com/STMicroelectronics/stm32f4xx_hal_driver.git',
                                       '04e99fbdabd00ab8f370f377c66b0a4570365b58',
                                       'stm32f4'],
    'hw/mcu/st/stm32f7xx_hal_driver': ['https://github.com/STMicroelectronics/stm32f7xx_hal_driver.git',
                                       'f7ffdf6bf72110e58b42c632b0a051df5997e4ee',
                                       'stm32f7'],
    'hw/mcu/st/stm32g0xx_hal_driver': ['https://github.com/STMicroelectronics/stm32g0xx_hal_driver.git',
                                       'e911b12c7f67084d7f6b76157a4c0d4e2ec3779c',
                                       'stm32g0'],
    'hw/mcu/st/stm32g4xx_hal_driver': ['https://github.com/STMicroelectronics/stm32g4xx_hal_driver.git',
                                       '8b4518417706d42eef5c14e56a650005abf478a8',
                                       'stm32g4'],
    'hw/mcu/st/stm32h7xx_hal_driver': ['https://github.com/STMicroelectronics/stm32h7xx_hal_driver.git',
                                       'd8461b980b59b1625207d8c4f2ce0a9c2a7a3b04',
                                       'stm32h7'],
    'hw/mcu/st/stm32h5xx_hal_driver': ['https://github.com/STMicroelectronics/stm32h5xx_hal_driver.git',
                                       '2cf77de584196d619cec1b4586c3b9e2820a254e',
                                       'stm32h5'],
    'hw/mcu/st/stm32l0xx_hal_driver': ['https://github.com/STMicroelectronics/stm32l0xx_hal_driver.git',
                                       'fbdacaf6f8c82a4e1eb9bd74ba650b491e97e17b',
                                       'stm32l0'],
    'hw/mcu/st/stm32l1xx_hal_driver': ['https://github.com/STMicroelectronics/stm32l1xx_hal_driver.git',
                                       '44efc446fa69ed8344e7fd966e68ed11043b35d9',
                                       'stm32l1'],
    'hw/mcu/st/stm32l4xx_hal_driver': ['https://github.com/STMicroelectronics/stm32l4xx_hal_driver.git',
                                       'aee3d5bf283ae5df87532b781bdd01b7caf256fc',
                                       'stm32l4'],
    'hw/mcu/st/stm32l5xx_hal_driver': ['https://github.com/STMicroelectronics/stm32l5xx_hal_driver.git',
                                       '675c32a75df37f39d50d61f51cb0dcf53f07e1cb',
                                       'stm32l5'],
    'hw/mcu/st/stm32u5xx_hal_driver': ['https://github.com/STMicroelectronics/stm32u5xx_hal_driver.git',
                                       '4d93097a67928e9377e655ddd14622adc31b9770',
                                       'stm32u5'],
    'hw/mcu/st/stm32wbxx_hal_driver': ['https://github.com/STMicroelectronics/stm32wbxx_hal_driver.git',
                                       '2c5f06638be516c1b772f768456ba637f077bac8',
                                       'stm32wb'],
    'hw/mcu/ti': ['https://github.com/hathach/ti_driver.git',
                  '143ed6cc20a7615d042b03b21e070197d473e6e5',
                  'msp430 msp432e4 tm4c'],
    'hw/mcu/wch/ch32v103': ['https://github.com/openwch/ch32v103.git',
                            '7578cae0b21f86dd053a1f781b2fc6ab99d0ec17',
                            'ch32v10x'],
    'hw/mcu/wch/ch32v20x': ['https://github.com/openwch/ch32v20x.git',
                            'c4c38f507e258a4e69b059ccc2dc27dde33cea1b',
                            'ch32v20x'],
    'hw/mcu/wch/ch32v307': ['https://github.com/openwch/ch32v307.git',
                            '184f21b852cb95eed58e86e901837bc9fff68775',
                            'ch32v307'],
    'hw/mcu/wch/ch32f20x': ['https://github.com/openwch/ch32f20x.git',
                            '77c4095087e5ed2c548ec9058e655d0b8757663b',
                            'ch32f20x'],
    'lib/CMSIS_5': ['https://github.com/ARM-software/CMSIS_5.git',
                    '20285262657d1b482d132d20d755c8c330d55c1f',
                    'imxrt kinetis_k32l2 kinetis_kl lpc51 lpc54 lpc55 mcx mm32 msp432e4 nrf ra saml2x'
                    'lpc11 lpc13 lpc15 lpc17 lpc18 lpc40 lpc43'
                    'stm32f0 stm32f1 stm32f2 stm32f3 stm32f4 stm32f7 stm32g0 stm32g4 stm32h5'
                    'stm32h7 stm32l0 stm32l1 stm32l4 stm32l5 stm32u5 stm32wb'
                    'sam3x samd11 samd21 samd51 samd5x_e5x same5x same7x saml2x samg'
                    'tm4c'],
    'lib/sct_neopixel': ['https://github.com/gsteiert/sct_neopixel.git',
                         'e73e04ca63495672d955f9268e003cffe168fcd8',
                         'lpc55'],
}

# combined 2 deps
deps_all = {**deps_mandatory, **deps_optional}

# TOP is tinyusb root dir
TOP = Path(__file__).parent.parent.resolve()


def run_cmd(cmd):
    return subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def get_a_dep(d):
    if d not in deps_all.keys():
        print('{} is not found in dependency list')
        return 1
    url = deps_all[d][0]
    commit = deps_all[d][1]
    families = deps_all[d][2]

    print(f'cloning {d} with {url}')

    p = Path(TOP / d)
    git_cmd = f"git -C {p}"

    # Init git deps if not existed
    if not p.exists():
        p.mkdir(parents=True)
        run_cmd(f"{git_cmd} init")
        run_cmd(f"{git_cmd} remote add origin {url}")

    # Check if commit is already fetched
    result = run_cmd(f"{git_cmd} rev-parse HEAD")
    head = result.stdout.decode("utf-8").splitlines()[0]
    run_cmd(f"{git_cmd} reset --hard")
    if commit != head:
        run_cmd(f"{git_cmd} fetch --depth 1 origin {commit}")
        run_cmd(f"{git_cmd} checkout FETCH_HEAD")

    return 0


def find_family(board):
    bsp_dir = Path(TOP / "hw/bsp")
    for family_dir in bsp_dir.iterdir():
        if family_dir.is_dir():
            board_dir = family_dir / 'boards' / board
            if board_dir.exists():
                return family_dir.name
    return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('families', nargs='*', default=[], help='Families to fetch')
    parser.add_argument('-b', '--board', action='append', default=[], help='Boards to fetch')
    parser.add_argument('--print', action='store_true', help='Print commit hash only')
    args = parser.parse_args()

    families = args.families
    boards = args.board
    print_only = args.print

    status = 0
    deps = list(deps_mandatory.keys())

    if 'all' in families:
        deps += deps_optional.keys()
    else:
        families = list(families)
        if boards is not None:
            for b in boards:
                f = find_family(b)
                if f is not None:
                    families.append(f)

        for f in families:
            for d in deps_optional:
                if d not in deps and f in deps_optional[d][2]:
                    deps.append(d)

    if print_only:
        pvalue = {}
        # print only without arguments, always add CMSIS_5
        if len(families) == 0 and len(boards) == 0:
            deps.append('lib/CMSIS_5')
        for d in deps:
            commit = deps_all[d][1]
            pvalue[d] = commit
        print(pvalue)
    else:
        with Pool() as pool:
            status = sum(pool.map(get_a_dep, deps))
    return status


if __name__ == "__main__":
    sys.exit(main())
