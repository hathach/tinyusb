#!/usr/bin/env python3
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
    'hw/mcu/analog/msdk' : ['https://github.com/analogdevicesinc/msdk.git',
                             'b20b398d3e5e2007594e54a74ba3d2a2e50ddd75',
                             'maxim'],
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
                           '11f57e578c7feea13f21c79ea0efab2630ac68c7',
                           'nrf'],
    'hw/mcu/nuvoton': ['https://github.com/majbthrd/nuc_driver.git',
                       '2204191ec76283371419fbcec207da02e1bc22fa',
                       'nuc100_120 nuc121_125 nuc126 nuc505'],
    'hw/mcu/nxp/lpcopen': ['https://github.com/hathach/nxp_lpcopen.git',
                           'b41cf930e65c734d8ec6de04f1d57d46787c76ae',
                           'lpc11 lpc13 lpc15 lpc17 lpc18 lpc40 lpc43'],
    'hw/mcu/nxp/mcux-sdk': ['https://github.com/nxp-mcuxpresso/mcux-sdk',
                            'a1bdae309a14ec95a4f64a96d3315a4f89c397c6',
                            'kinetis_k kinetis_k32l2 kinetis_kl lpc51 lpc54 lpc55 mcx imxrt'],
    'hw/mcu/raspberry_pi/Pico-PIO-USB': ['https://github.com/sekigon-gonnoc/Pico-PIO-USB.git',
                                         '675543bcc9baa8170f868ab7ba316d418dbcf41f',
                                         'rp2040'],
    'hw/mcu/renesas/fsp': ['https://github.com/renesas/fsp.git',
                           'edcc97d684b6f716728a60d7a6fea049d9870bd6',
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
    'hw/mcu/st/cmsis_device_c0': ['https://github.com/STMicroelectronics/cmsis_device_c0.git',
                                  '517611273f835ffe95318947647bc1408f69120d',
                                  'stm32c0'],
    'hw/mcu/st/cmsis_device_f0': ['https://github.com/STMicroelectronics/cmsis_device_f0.git',
                                  'cbb5da5d48b4b5f2efacdc2f033be30f9d29889f',
                                  'stm32f0'],
    'hw/mcu/st/cmsis_device_f1': ['https://github.com/STMicroelectronics/cmsis_device_f1.git',
                                  'c8e9a4a4f16b6d2cb2a2083cbe5161025280fb22',
                                  'stm32f1'],
    'hw/mcu/st/cmsis_device_f2': ['https://github.com/STMicroelectronics/cmsis_device_f2.git',
                                  '49321f1e4d2bd3e65687b37f2652a28ea7983674',
                                  'stm32f2'],
    'hw/mcu/st/cmsis_device_f3': ['https://github.com/STMicroelectronics/cmsis_device_f3.git',
                                  '5558e64e3675a1e1fcb1c71f468c7c407c1b1134',
                                  'stm32f3'],
    'hw/mcu/st/cmsis_device_f4': ['https://github.com/STMicroelectronics/cmsis_device_f4.git',
                                  '3c77349ce04c8af401454cc51f85ea9a50e34fc1',
                                  'stm32f4'],
    'hw/mcu/st/cmsis_device_f7': ['https://github.com/STMicroelectronics/cmsis_device_f7.git',
                                  '2352e888e821aa0f4fe549bd5ea81d29c67a3222',
                                  'stm32f7'],
    'hw/mcu/st/cmsis_device_g0': ['https://github.com/STMicroelectronics/cmsis_device_g0.git',
                                  'f484fe852535f913a02ee79787eafa74dd7f9488',
                                  'stm32g0'],
    'hw/mcu/st/cmsis_device_g4': ['https://github.com/STMicroelectronics/cmsis_device_g4.git',
                                  '7c39c32593b03764aaa57531588b8bf7cdd443a5',
                                  'stm32g4'],
    'hw/mcu/st/cmsis_device_h7': ['https://github.com/STMicroelectronics/cmsis_device_h7.git',
                                  '45b818cab6ee2806e3a27c80e330957223424392',
                                  'stm32h7'],
    'hw/mcu/st/cmsis_device_h7rs': ['https://github.com/STMicroelectronics/cmsis_device_h7rs.git',
                                  '57ea11f70ebf1850e1048989d665c9070f0bb863',
                                  'stm32h7rs'],
    'hw/mcu/st/cmsis_device_h5': ['https://github.com/STMicroelectronics/cmsis_device_h5.git',
                                  '5273b8f134ba65f5b8174c4141b711b5c0d295b2',
                                  'stm32h5'],
    'hw/mcu/st/cmsis_device_l0': ['https://github.com/STMicroelectronics/cmsis_device_l0.git',
                                  '7b7ae8cd71437331e1d7824f157d00c7bb4a5044',
                                  'stm32l0'],
    'hw/mcu/st/cmsis_device_l1': ['https://github.com/STMicroelectronics/cmsis_device_l1.git',
                                  'a23ade4ccf14012085fedf862e33a536ab7ed8be',
                                  'stm32l1'],
    'hw/mcu/st/cmsis_device_l4': ['https://github.com/STMicroelectronics/cmsis_device_l4.git',
                                  'a2530753e86dd326a75467d28feb92e2ba7d0df2',
                                  'stm32l4'],
    'hw/mcu/st/cmsis_device_l5': ['https://github.com/STMicroelectronics/cmsis_device_l5.git',
                                  '7d9a51481f0e6c376e62c3c849e6caf652c66482',
                                  'stm32l5'],
    'hw/mcu/st/cmsis_device_n6': ['https://github.com/STMicroelectronics/cmsis-device-n6.git',
                                  '7bcdc944fbf7cf5928d3c1d14054ca13261d33ec',
                                  'stm32n6'],
    'hw/mcu/st/cmsis-device-u0': ['https://github.com/STMicroelectronics/cmsis-device-u0.git',
                                  'e3a627c6a5bc4eb2388e1885a95cc155e1672253',
                                  'stm32u0'],
    'hw/mcu/st/cmsis_device_u5': ['https://github.com/STMicroelectronics/cmsis_device_u5.git',
                                  '6e67187dec98035893692ab2923914cb5f4e0117',
                                  'stm32u5'],
    'hw/mcu/st/cmsis_device_wb': ['https://github.com/STMicroelectronics/cmsis_device_wb.git',
                                  'cda2cb9fc4a5232ab18efece0bb06b0b60910083',
                                  'stm32wb'],
    'hw/mcu/st/cmsis-device-wba': ['https://github.com/STMicroelectronics/cmsis-device-wba.git',
                                   '647d8522e5fd15049e9a1cc30ed19d85e5911eaf',
                                   'stm32wba'],
    'hw/mcu/st/stm32-mfxstm32l152': ['https://github.com/STMicroelectronics/stm32-mfxstm32l152.git',
                                     '7f4389efee9c6a655b55e5df3fceef5586b35f9b',
                                     'stm32h7'],
    'hw/mcu/st/stm32-tcpp0203': ['https://github.com/STMicroelectronics/stm32-tcpp0203.git',
                                 '9918655bff176ac3046ccf378b5c7bbbc6a38d15',
                                 'stm32h7rs stm32n6'],
    'hw/mcu/st/stm32c0xx_hal_driver': ['https://github.com/STMicroelectronics/stm32c0xx_hal_driver.git',
                                       'c283b143bef6bdaacf64240ee6f15eb61dad6125',
                                       'stm32c0'],
    'hw/mcu/st/stm32f0xx_hal_driver': ['https://github.com/STMicroelectronics/stm32f0xx_hal_driver.git',
                                       '94399697cb5eeaf8511b81b7f50dc62f0a5a3f6c',
                                       'stm32f0'],
    'hw/mcu/st/stm32f1xx_hal_driver': ['https://github.com/STMicroelectronics/stm32f1xx_hal_driver.git',
                                       '18074e3e5ecad0b380a5cf5a9131fe4b5ed1b2b7',
                                       'stm32f1'],
    'hw/mcu/st/stm32f2xx_hal_driver': ['https://github.com/STMicroelectronics/stm32f2xx_hal_driver.git',
                                       'ae7b47fe41cf75ccaf65cbf8ee8749b18ba0e0f3',
                                       'stm32f2'],
    'hw/mcu/st/stm32f3xx_hal_driver': ['https://github.com/STMicroelectronics/stm32f3xx_hal_driver.git',
                                       'e098c8c8ce6f426bcee7db3a37c0932ea881eb0b',
                                       'stm32f3'],
    'hw/mcu/st/stm32f4xx_hal_driver': ['https://github.com/STMicroelectronics/stm32f4xx_hal_driver.git',
                                       'b6f0ed3829f3829eb358a2e7417d80bba1a42db7',
                                       'stm32f4'],
    'hw/mcu/st/stm32f7xx_hal_driver': ['https://github.com/STMicroelectronics/stm32f7xx_hal_driver.git',
                                       'e1446fa12ffda80ea1016faf349e45b2047fff12',
                                       'stm32f7'],
    'hw/mcu/st/stm32g0xx_hal_driver': ['https://github.com/STMicroelectronics/stm32g0xx_hal_driver.git',
                                       'a248a9e484d58943b46c68f6c49b4b276778bd59',
                                       'stm32g0'],
    'hw/mcu/st/stm32g4xx_hal_driver': ['https://github.com/STMicroelectronics/stm32g4xx_hal_driver.git',
                                       '10138a41749ea62d53ecab65b2bc2a950acc04d2',
                                       'stm32g4'],
    'hw/mcu/st/stm32h7xx_hal_driver': ['https://github.com/STMicroelectronics/stm32h7xx_hal_driver.git',
                                       'dbfb749f229e1aa89e50b54229ca87766e180d2d',
                                       'stm32h7'],
    'hw/mcu/st/stm32h7rsxx_hal_driver': ['https://github.com/STMicroelectronics/stm32h7rsxx-hal-driver.git',
                                       '9e83b95ae0f70faa067eddce2da617d180937f9b',
                                       'stm32h7rs'],
    'hw/mcu/st/stm32h5xx_hal_driver': ['https://github.com/STMicroelectronics/stm32h5xx_hal_driver.git',
                                       '3c84eaa6000ab620be01afbcfba2735389afe09b',
                                       'stm32h5'],
    'hw/mcu/st/stm32l0xx_hal_driver': ['https://github.com/STMicroelectronics/stm32l0xx_hal_driver.git',
                                       '65da4cd8a10ad859ec8d9cd71f3f6c50735bd473',
                                       'stm32l0'],
    'hw/mcu/st/stm32l1xx_hal_driver': ['https://github.com/STMicroelectronics/stm32l1xx_hal_driver.git',
                                       '54f0b7568ce2acb33d090c70c897ee32229c1d32',
                                       'stm32l1'],
    'hw/mcu/st/stm32l4xx_hal_driver': ['https://github.com/STMicroelectronics/stm32l4xx_hal_driver.git',
                                       '3e039bbf62f54bbd834d578185521cff80596efe',
                                       'stm32l4'],
    'hw/mcu/st/stm32l5xx_hal_driver': ['https://github.com/STMicroelectronics/stm32l5xx_hal_driver.git',
                                       '3340b9a597bcf75cc173345a90a74aa2a4a37510',
                                       'stm32l5'],
    'hw/mcu/st/stm32n6xx_hal_driver': ['https://github.com/STMicroelectronics/stm32n6xx-hal-driver.git',
                                       'bc6c41f8f67d61b47af26695d0bf67762a000666',
                                       'stm32n6'],
    'hw/mcu/st/stm32u0xx_hal_driver': ['https://github.com/STMicroelectronics/stm32u0xx-hal-driver.git',
                                       'cbfb5ac654256445237fd32b3587ac6a238d24f1',
                                       'stm32u0'],
    'hw/mcu/st/stm32u5xx_hal_driver': ['https://github.com/STMicroelectronics/stm32u5xx_hal_driver.git',
                                       '2c5e2568fbdb1900a13ca3b2901fdd302cac3444',
                                       'stm32u5'],
    'hw/mcu/st/stm32wbxx_hal_driver': ['https://github.com/STMicroelectronics/stm32wbxx_hal_driver.git',
                                       'd60dd46996876506f1d2e9abd6b1cc110c8004cd',
                                       'stm32wb'],
    'hw/mcu/st/stm32wbaxx_hal_driver': ['https://github.com/STMicroelectronics/stm32wbaxx_hal_driver.git',
                                       '9442fbb71f855ff2e64fbf662b7726beba511a24',
                                       'stm32wba'],
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
                            'ch32v30x'],
    'hw/mcu/wch/ch32f20x': ['https://github.com/openwch/ch32f20x.git',
                            '77c4095087e5ed2c548ec9058e655d0b8757663b',
                            'ch32f20x'],
    'hw/mcu/artery/at32f403a_407': ['https://github.com/ArteryTek/AT32F403A_407_Firmware_Library.git',
                                    'f2cb360c3d28fada76b374308b8c4c61d37a090b',
                                    'at32f403a_407'],
    'hw/mcu/artery/at32f415': ['https://github.com/ArteryTek/AT32F415_Firmware_Library.git',
                               '716f545aa1290ff144ccf023a8e797b951e1bc8e',
                               'at32f415'],
    'hw/mcu/artery/at32f435_437': ['https://github.com/ArteryTek/AT32F435_437_Firmware_Library.git',
                                   '25439cc6650a8ae0345934e8707a5f38c7ae41f8',
                                   'at32f435_437'],
    'hw/mcu/artery/at32f423': ['https://github.com/ArteryTek/AT32F423_Firmware_Library.git',
                               '2afa7f12852e57a9e8aab3a892c641e1a8635a18',
                               'at32f423'],
    'hw/mcu/artery/at32f402_405': ['https://github.com/ArteryTek/AT32F402_405_Firmware_Library.git',
                                   '4424515c2663e82438654e0947695295df2abdfe',
                                   'at32f402_405'],
    'hw/mcu/artery/at32f425': ['https://github.com/ArteryTek/AT32F425_Firmware_Library.git',
                               '620233e1357d5c1b7e2bde6b9dd5196822b91817',
                               'at32f425'],
    'hw/mcu/artery/at32f413': ['https://github.com/ArteryTek/AT32F413_Firmware_Library.git',
                               'f6fe62dfec9fd40c5b63d92fc5ef2c2b5e77a450',
                               'at32f413'],
    'lib/CMSIS_5': ['https://github.com/ARM-software/CMSIS_5.git',
                    '2b7495b8535bdcb306dac29b9ded4cfb679d7e5c',
                    'imxrt kinetis_k32l2 kinetis_kl lpc51 lpc54 lpc55 mcx mm32 msp432e4 nrf saml2x '
                    'lpc11 lpc13 lpc15 lpc17 lpc18 lpc40 lpc43 '
                    'stm32c0 stm32f0 stm32f1 stm32f2 stm32f3 stm32f4 stm32f7 stm32g0 stm32g4 stm32h5 '
                    'stm32h7 stm32h7rs stm32l0 stm32l1 stm32l4 stm32l5 stm32u0 stm32u5 stm32wb stm32wba'
                    'sam3x samd11 samd21 samd51 samd5x_e5x same5x same7x saml2x samg '
                    'tm4c '],
    'lib/CMSIS_6': ['https://github.com/ARM-software/CMSIS_6.git',
                    '6f0a58d01aa9bd2feba212097f9afe7acd991d52',
                    'ra stm32n6'],
    'lib/sct_neopixel': ['https://github.com/gsteiert/sct_neopixel.git',
                         'e73e04ca63495672d955f9268e003cffe168fcd8',
                         'lpc55'],
}

# combined 2 deps
deps_all = {**deps_mandatory, **deps_optional}

# TOP is tinyusb root dir
TOP = Path(__file__).parent.parent.resolve()


def run_cmd(cmd):
    r = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    title = f'Command Error: {cmd}'
    if r.returncode != 0:
        print(title)
        print(r.stdout.decode("utf-8"))
    return r


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
        head = None
    else:
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
    parser.add_argument('-D', '--define', action='append', default=[], help='Have no effect')
    parser.add_argument('-f1', '--build-flags-on', action='append', default=[], help='Have no effect')
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
