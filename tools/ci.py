import click
import json

# family: toolchain
family_list = {
    "broadcom_32bit": ["arm-gcc"],
    "imxrt": ["arm-gcc", "arm-clang"],
    "kinetis_k kinetis_kl kinetis_k32l2": ["arm-gcc", "arm-clang"],
    "lpc11 lpc13 lpc15": ["arm-gcc"],
    "lpc17 lpc18 lpc40 lpc43": ["arm-gcc", "arm-clang"],
    "lpc51 lpc54 lpc55": ["arm-gcc", "arm-clang"],
    "mcx": ["arm-gcc"],
    "mm32": ["arm-gcc"],
    "msp432e4": ["arm-gcc"],
    "nrf": ["arm-gcc", "arm-clang"],
    "ra": ["arm-gcc"],
    "rp2040": ["arm-gcc"],
    "samd11 samd21 saml2x samd5x_e5x samg": ["arm-gcc", "arm-clang"],
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
    print(json.dumps({"family": filtered_families}, indent=2))


if __name__ == '__main__':
    get_family_matrix()
