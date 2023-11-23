import click
import ctypes
import pandas as pd

# hex value for register: guid, gsnpsid, ghwcfg1, ghwcfg2, ghwcfg3, ghwcfg4
dwc2_reg_list = ['guid', 'gsnpsid', 'ghwcfg1', 'ghwcfg2', 'ghwcfg3', 'ghwcfg4']
dwc2_reg_value = {
    'BCM2711 (Pi4)': [0x2708A000, 0x4F54280A, 0, 0x228DDD50, 0xFF000E8, 0x1FF00020],
    'EFM32GG FullSpeed': [0, 0x4F54330A, 0, 0x228F5910, 0x1F204E8, 0x1BF08030],
    'ESP32-S2': [0, 0x4F54400A, 0, 0x224DD930, 0xC804B5, 0xD3F0A030],
    'STM32F407 Fullspeed': [0x1200, 0x4F54281A, 0, 0x229DCD20, 0x20001E8, 0xFF08030],
    'STM32F407 Highspeed': [0x1100, 0x4F54281A, 0, 0x229ED590, 0x3F403E8, 0x17F00030],
    'STM32F411 Fullspeed': [0x1200, 0x4F54281A, 0, 0x229DCD20, 0x20001E8, 0xFF08030],
    'STM32F412 Fullspeed': [0x2000, 0x4F54320A, 0, 0x229ED520, 0x200D1E8, 0x17F08030],
    'STM32F429 Fullspeed': [0x1200, 0x4F54281A, 0, 0x229DCD20, 0x20001E8, 0xFF08030],
    'STM32F429 Highspeed': [0x1100, 0x4F54281A, 0, 0x229ED590, 0x3F403E8, 0x17F00030],
    'STM32F723 Fullspeed': [0x3000, 0x4F54330A, 0, 0x229ED520, 0x200D1E8, 0x17F08030],
    'STM32F723 HighSpeed': [0x3100, 0x4F54330A, 0, 0x229FE1D0, 0x3EED2E8, 0x23F00030],
    'STM32F767 Fullspeed': [0x2000, 0x4F54320A, 0, 0x229ED520, 0x200D1E8, 0x17F08030],
    'STM32H743 Highspeed': [0x2300, 0x4F54330A, 0, 0x229FE190, 0x3B8D2E8, 0xE3F00030], # both HS cores
    'STM32L476 Fullspeed': [0x2000, 0x4F54310A, 0, 0x229ED520, 0x200D1E8, 0x17F08030],
    'STM32U5A5 Highspeed': [0x00005000, 0x4F54411A, 0x00000000, 0x228FE052, 0x03B882E8, 0xE2103E30],
    'GD32VF103 Fullspeed': [0x1000, 0, 0, 0, 0, 0],
    'XMC4500': [0xAEC000, 0x4F54292A, 0, 0x228F5930, 0x27A01E5, 0xDBF08030]
}

# Combine dwc2_info with dwc2_reg_list
# dwc2_info = {
#     'BCM2711 (Pi4)': {
#         'guid': 0x2708A000,
#         'gsnpsid': 0x4F54280A,
#         'ghwcfg1': 0,
#         'ghwcfg2': 0x228DDD50,
#         'ghwcfg3': 0xFF000E8,
#         'ghwcfg4': 0x1FF00020
#     },
dwc2_info = {key: {field: value for field, value in zip(dwc2_reg_list, values)} for key, values in dwc2_reg_value.items()}


class GHWCFG2(ctypes.LittleEndianStructure):
    _fields_ = [
        ("op_mode", ctypes.c_uint32, 3),
        ("arch", ctypes.c_uint32, 2),
        ("point2point", ctypes.c_uint32, 1),
        ("hs_phy_type", ctypes.c_uint32, 2),
        ("fs_phy_type", ctypes.c_uint32, 2),
        ("num_dev_ep", ctypes.c_uint32, 4),
        ("num_host_ch", ctypes.c_uint32, 4),
        ("period_channel_support", ctypes.c_uint32, 1),
        ("enable_dynamic_fifo", ctypes.c_uint32, 1),
        ("mul_cpu_int", ctypes.c_uint32, 1),
        ("reserved21", ctypes.c_uint32, 1),
        ("nperiod_tx_q_depth", ctypes.c_uint32, 2),
        ("host_period_tx_q_depth", ctypes.c_uint32, 2),
        ("dev_token_q_depth", ctypes.c_uint32, 5),
        ("otg_enable_ic_usb", ctypes.c_uint32, 1)
    ]


class GHWCFG3(ctypes.LittleEndianStructure):
    _fields_ = [
        ("xfer_size_width", ctypes.c_uint32, 4),
        ("packet_size_width", ctypes.c_uint32, 3),
        ("otg_enable", ctypes.c_uint32, 1),
        ("i2c_enable", ctypes.c_uint32, 1),
        ("vendor_ctrl_itf", ctypes.c_uint32, 1),
        ("optional_feature_removed", ctypes.c_uint32, 1),
        ("synch_reset", ctypes.c_uint32, 1),
        ("otg_adp_support", ctypes.c_uint32, 1),
        ("otg_enable_hsic", ctypes.c_uint32, 1),
        ("battery_charger_support", ctypes.c_uint32, 1),
        ("lpm_mode", ctypes.c_uint32, 1),
        ("total_fifo_size", ctypes.c_uint32, 16)
    ]


class GHWCFG4(ctypes.LittleEndianStructure):
    _fields_ = [
        ("num_dev_period_in_ep", ctypes.c_uint32, 4),
        ("power_optimized", ctypes.c_uint32, 1),
        ("ahb_freq_min", ctypes.c_uint32, 1),
        ("hibernation", ctypes.c_uint32, 1),
        ("reserved7", ctypes.c_uint32, 3),
        ("service_interval_mode", ctypes.c_uint32, 1),
        ("ipg_isoc_en", ctypes.c_uint32, 1),
        ("acg_enable", ctypes.c_uint32, 1),
        ("reserved13", ctypes.c_uint32, 1),
        ("utmi_phy_data_width", ctypes.c_uint32, 2),
        ("dev_ctrl_ep_num", ctypes.c_uint32, 4),
        ("iddg_filter_enabled", ctypes.c_uint32, 1),
        ("vbus_valid_filter_enabled", ctypes.c_uint32, 1),
        ("a_valid_filter_enabled", ctypes.c_uint32, 1),
        ("b_valid_filter_enabled", ctypes.c_uint32, 1),
        ("dedicated_fifos", ctypes.c_uint32, 1),
        ("num_dev_in_eps", ctypes.c_uint32, 4),
        ("dma_desc_enable", ctypes.c_uint32, 1),
        ("dma_dynamic", ctypes.c_uint32, 1)
    ]


@click.group()
def cli():
    pass


@cli.command()
@click.argument('mcus', nargs=-1)
@click.option('-a', '--all', is_flag=True, help='Print all bit-field values')
def info(mcus, all):
    """Print DWC2 register values for given MCU(s)"""
    if len(mcus) == 0:
        mcus = dwc2_info

    for mcu in mcus:
        for entry in dwc2_info:
            if mcu.lower() in entry.lower():
                print(f"## {entry}")
                for r_name, r_value in dwc2_info[entry].items():
                    print(f"{r_name} = 0x{r_value:08X}")
                    # Print bit-field values
                    if all and r_name.upper() in globals():
                        class_name = globals()[r_name.upper()]
                        ghwcfg = class_name.from_buffer_copy(r_value.to_bytes(4, byteorder='little'))
                        for field_name, field_type, _ in class_name._fields_:
                            print(f"  {field_name} = {getattr(ghwcfg, field_name)}")


@cli.command()
def render_md():
    """Render dwc2_info to Markdown table"""
    # Create an empty list to hold the dictionaries
    dwc2_info_list = []

    # Iterate over the dwc2_info dictionary and extract fields
    for device, reg_values in dwc2_info.items():
        entry_dict = {"Device": device}
        for r_name, r_value in reg_values.items():
            entry_dict[r_name] = f"0x{r_value:08X}"

            if r_name == 'gsnpsid':
                # Get dwc2 specs version
                major = ((r_value >> 8) >> 4) & 0x0F
                minor = (r_value >> 4) & 0xFF
                patch = chr((r_value & 0x0F) + ord('a') - 0xA)
                entry_dict[f' - specs version'] = f"{major:X}.{minor:02X}{patch}"
            elif r_name.upper() in globals():
                # Get bit-field values which exist as ctypes structures
                class_name = globals()[r_name.upper()]
                ghwcfg = class_name.from_buffer_copy(r_value.to_bytes(4, byteorder='little'))
                for field_name, field_type, _ in class_name._fields_:
                    entry_dict[f' - {field_name}'] = getattr(ghwcfg, field_name)

        dwc2_info_list.append(entry_dict)

    # Create a Pandas DataFrame from the list of dictionaries
    df = pd.DataFrame(dwc2_info_list).set_index('Device')

    # Transpose the DataFrame to switch rows and columns
    df = df.T
    #print(df)

    # Write the Markdown table to a file
    with open('dwc2_info.md', 'w') as md_file:
        md_file.write(df.to_markdown())
        md_file.write('\n')


if __name__ == '__main__':
    cli()
