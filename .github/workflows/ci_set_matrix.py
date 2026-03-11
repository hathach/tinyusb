#!/usr/bin/env python3
import json

# toolchain, url
toolchain_list = [
    "aarch64-gcc",
    "arm-clang",
    "arm-iar",
    "arm-gcc",
    "esp-idf",
    "msp430-gcc",
    "riscv-gcc",
    "rx-gcc"
]

# family: [supported toolchain]
family_list = {
    "at32f402_405": ["arm-gcc"],
    "at32f403a_407": ["arm-gcc"],
    "at32f413": ["arm-gcc"],
    "at32f415": ["arm-gcc"],
    "at32f423": ["arm-gcc"],
    "at32f425": ["arm-gcc"],
    "at32f435_437": ["arm-gcc"],
    "at32f45x": ["arm-gcc"],
    "broadcom_32bit": ["arm-gcc"],
    "broadcom_64bit": ["aarch64-gcc"],
    "ch32v10x": ["riscv-gcc"],
    "ch32v20x": ["riscv-gcc"],
    "ch32v30x": ["riscv-gcc"],
    "da1469x": ["arm-gcc"],
    "fomu": ["riscv-gcc"],
    "gd32vf103": ["riscv-gcc"],
    "hpmicro": ["riscv-gcc"],
    "imxrt": ["arm-gcc", "arm-clang"],
    "kinetis_k": ["arm-gcc", "arm-clang"],
    "kinetis_k32l2": ["arm-gcc", "arm-clang"],
    "kinetis_kl": ["arm-gcc", "arm-clang"],
    "lpc11": ["arm-gcc", "arm-clang"],
    "lpc13": ["arm-gcc", "arm-clang"],
    "lpc15": ["arm-gcc", "arm-clang"],
    "lpc17": ["arm-gcc", "arm-clang"],
    "lpc18": ["arm-gcc", "arm-clang"],
    "lpc40": ["arm-gcc", "arm-clang"],
    "lpc43": ["arm-gcc", "arm-clang"],
    "lpc51": ["arm-gcc", "arm-clang"],
    "lpc54": ["arm-gcc", "arm-clang"],
    "lpc55": ["arm-gcc", "arm-clang"],
    "maxim": ["arm-gcc"],
    "mcx": ["arm-gcc"],
    "mm32": ["arm-gcc"],
    "msp430": ["msp430-gcc"],
    "msp432e4": ["arm-gcc"],
    "nrf": ["arm-gcc", "arm-clang"],
    "nuc100_120": ["arm-gcc"],
    "nuc121_125": ["arm-gcc"],
    "nuc126": ["arm-gcc"],
    "nuc505": ["arm-gcc"],
    "ra": ["arm-gcc"],
    "rp2040": ["arm-gcc"],
    "rw61x": ["arm-gcc"],
    "rx": ["rx-gcc"],
    "samd11": ["arm-gcc", "arm-clang"],
    "samd2x_l2x": ["arm-gcc", "arm-clang"],
    "samd5x_e5x": ["arm-gcc", "arm-clang"],
    "samg": ["arm-gcc", "arm-clang"],
    "stm32c0": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32f0": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32f1": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32f2": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32f3": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32f4": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32f7": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32g0": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32g4": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32h5": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32h7": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32h7rs": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32l0": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32l4": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32n6": ["arm-gcc"],
    "stm32u0": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32u5": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32wb": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32wba": ["arm-gcc", "arm-clang", "arm-iar"],
    "tm4c": ["arm-gcc"],
    "xmc4000": ["arm-gcc"],
    # S3, P4 will be built by hil test
    # "-bespressif_s3_devkitm": ["esp-idf"],
    # "-bespressif_p4_function_ev": ["esp-idf"],
}


def set_matrix_json():
    matrix = {}
    for toolchain in toolchain_list:
        filtered_families = [family for family, supported_toolchain in family_list.items() if
                             toolchain in supported_toolchain]
        matrix[toolchain] = filtered_families

    print(json.dumps(matrix))


if __name__ == '__main__':
    set_matrix_json()
