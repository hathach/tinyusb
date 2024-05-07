import click
import json

# family: toolchain
family_list = {
    "broadcom_32bit": ["armgcc"],
    "imxrt": ["armgcc", "armclang"],
    "kinetis_k kinetis_kl kinetis_k32l2": ["armgcc", "armclang"],
    "lpc11 lpc13 lpc15": ["armgcc"],
    "lpc17 lpc18 lpc40 lpc43": ["armgcc", "armclang"],
    "lpc51 lpc54 lpc55": ["armgcc", "armclang"],
    "mcx": ["armgcc"],
    "mm32": ["armgcc"],
    "msp432e4": ["armgcc"],
    "nrf": ["armgcc", "armclang"],
    "ra": ["armgcc"],
    "rp2040": ["armgcc"],
    "samd11 samd21 saml2x samd5x_e5x samg": ["armgcc", "armclang"],
    "stm32f0": ["armgcc", "armclang"],
    "stm32f1": ["armgcc", "armclang"],
    "stm32f2": ["armgcc", "armclang"],
    "stm32f3": ["armgcc", "armclang"],
    "stm32f4": ["armgcc", "armclang"],
    "stm32f7": ["armgcc", "armclang"],
    "stm32g0": ["armgcc", "armclang"],
    "stm32g4": ["armgcc", "armclang"],
    "stm32h5": ["armgcc", "armclang"],
    "stm32h7": ["armgcc", "armclang"],
    "stm32l4": ["armgcc", "armclang"],
    "stm32u5": ["armgcc", "armclang"],
    "stm32wb": ["armgcc"],
    "tm4c": ["armgcc"],
    "xmc4000": ["armgcc"],
}


@click.command()
@click.option('--toolchain', default='armgcc', help='Toolchain to filter by.')
def get_family_matrix(toolchain):
    filtered_families = [family for family, toolchains in family_list.items() if toolchain in toolchains]
    print(json.dumps({"family": filtered_families}, indent=2))


if __name__ == '__main__':
    get_family_matrix()
