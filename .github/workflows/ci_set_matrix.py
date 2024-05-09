import json

# toolchain, url
toolchain_list = {
    "aarch64-gcc": "https://developer.arm.com/-/media/Files/downloads/gnu-a/10.3-2021.07/binrel/gcc-arm-10.3-2021.07-x86_64-aarch64-none-elf.tar.xz",
    "arm-clang": "https://github.com/ARM-software/LLVM-embedded-toolchain-for-Arm/releases/download/release-17.0.1/LLVMEmbeddedToolchainForArm-17.0.1-Linux-x86_64.tar.xz",
    "arm-gcc": "",
    "msp430-gcc": "http://software-dl.ti.com/msp430/msp430_public_sw/mcu/msp430/MSPGCC/9_2_0_0/export/msp430-gcc-9.2.0.50_linux64.tar.bz2",
    "riscv-gcc": "https://github.com/xpack-dev-tools/riscv-none-embed-gcc-xpack/releases/download/v10.1.0-1.1/xpack-riscv-none-embed-gcc-10.1.0-1.1-linux-x64.tar.gz",
}

# family: [supported toolchain]
family_list = {
    "broadcom_32bit": ["arm-gcc"],
    "broadcom_64bit": ["aarch64-gcc"],
    "ch32v307 fomu gd32vf103": ["riscv-gcc"],
    "imxrt": ["arm-gcc", "arm-clang"],
    "kinetis_k kinetis_kl kinetis_k32l2": ["arm-gcc", "arm-clang"],
    "lpc11 lpc13 lpc15": ["arm-gcc"],
    "lpc17 lpc18 lpc40 lpc43": ["arm-gcc", "arm-clang"],
    "lpc51 lpc54 lpc55": ["arm-gcc", "arm-clang"],
    "mcx": ["arm-gcc"],
    "mm32": ["arm-gcc"],
    "msp430": ["msp430-gcc"],
    "msp432e4 tm4c": ["arm-gcc"],
    "nrf": ["arm-gcc", "arm-clang"],
    "ra": ["arm-gcc"],
    "rp2040": ["arm-gcc"],
    "samd11 samd21 saml2x": ["arm-gcc", "arm-clang"],
    "samd5x_e5x samg": ["arm-gcc", "arm-clang"],
    "stm32f0 stm32f1 stm32f2 stm32f3": ["arm-gcc", "arm-clang"],
    "stm32f4": ["arm-gcc", "arm-clang"],
    "stm32f7": ["arm-gcc", "arm-clang"],
    "stm32g0 stm32g4 stm32h5": ["arm-gcc", "arm-clang"],
    "stm32h7": ["arm-gcc", "arm-clang"],
    "stm32l4 stm32u5 stm32wb": ["arm-gcc", "arm-clang"],
    "xmc4000": ["arm-gcc"],
}


def set_matrix_json():
    matrix = {}
    for toolchain in toolchain_list.keys():
        filtered_families = [family for family, supported_toolchain in family_list.items() if
                             toolchain in supported_toolchain]
        matrix[toolchain] = {"family": filtered_families, "toolchain_url": toolchain_list[toolchain]}
    print(json.dumps(matrix))


if __name__ == '__main__':
    set_matrix_json()
