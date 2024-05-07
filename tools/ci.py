import click
import json

# toolchain
toolchain_list = {
    "arm-gcc": {"url": ""},
    "arm-clang": {
        "url": "https://github.com/ARM-software/LLVM-embedded-toolchain-for-Arm/releases/download/release-17.0.1/LLVMEmbeddedToolchainForArm-17.0.1-Linux-x86_64.tar.xz"},
}

# family: toolchain
family_list = {
    "broadcom_32bit": ["arm-gcc"],
    "imxrt": ["arm-gcc", "arm-clang"],
    "kinetis_k": ["arm-gcc", "arm-clang"],
    "kinetis_kl": ["arm-gcc", "arm-clang"],
    "kinetis_k32l2": ["arm-gcc", "arm-clang"],
    "lpc11": ["arm-gcc"],
    "lpc13": ["arm-gcc"],
    "lpc15": ["arm-gcc"],
    "lpc17": ["arm-gcc", "arm-clang"],
    "lpc18": ["arm-gcc", "arm-clang"],
    "lpc40": ["arm-gcc", "arm-clang"],
    "lpc43": ["arm-gcc", "arm-clang"],
    "lpc51": ["arm-gcc", "arm-clang"],
    "lpc54": ["arm-gcc", "arm-clang"],
    "lpc55": ["arm-gcc", "arm-clang"],
    "mcx": ["arm-gcc"],
    "mm32": ["arm-gcc"],
    "msp430": ["msp430-gcc"],
    "msp432e4": ["arm-gcc"],
    "nrf": ["arm-gcc", "arm-clang"],
    "ra": ["arm-gcc"],
    "rp2040": ["arm-gcc"],
    "samd11": ["arm-gcc", "arm-clang"],
    "samd21": ["arm-gcc", "arm-clang"],
    "saml2x": ["arm-gcc", "arm-clang"],
    "samd5x_e5x": ["arm-gcc", "arm-clang"],
    "samg": ["arm-gcc", "arm-clang"],
    "stm32f0": ["arm-gcc", "arm-clang"],
    "stm32f1": ["arm-gcc", "arm-clang"],
    "stm32f2": ["arm-gcc", "arm-clang"],
    "stm32f3": ["arm-gcc", "arm-clang"],
    "stm32f4": ["arm-gcc", "arm-clang"],
    "stm32f7": ["arm-gcc", "arm-clang"],
    "stm32g0": ["arm-gcc", "arm-clang"],
    "stm32g4": ["arm-gcc", "arm-clang"],
    "stm32h5": ["arm-gcc", "arm-clang"],
    "stm32h7": ["arm-gcc", "arm-clang"],
    "stm32l4": ["arm-gcc", "arm-clang"],
    "stm32u5": ["arm-gcc", "arm-clang"],
    "stm32wb": ["arm-gcc"],
    "tm4c": ["arm-gcc"],
    "xmc4000": ["arm-gcc"],
}


@click.command()
@click.option('--toolchain', default='arm-gcc', help='Toolchain to filter by.')
def get_family_matrix(toolchain):
    filtered_families = [family for family, toolchains in family_list.items() if toolchain in toolchains]
    print(json.dumps({"family": filtered_families, "toolchain": toolchain, "toolchain_url" : toolchain_list[toolchain]}))


if __name__ == '__main__':
    get_family_matrix()
