# The MIT License (MIT)
#
# Copyright (c) 2022 Rafael Silva (@perigoso)
# Copyright (c) 2022 Ha Thach (tinyusb.org)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# This file is part of the TinyUSB stack.

tusb_mcu = tusb_mcu.to_upper()

tusb_inc = include_directories([
    'src'
])

tusb_src = files([
    'src/class/audio/audio_device.c',
    'src/class/bth/bth_device.c',
    'src/class/cdc/cdc_device.c',
    'src/class/cdc/cdc_host.c',
    'src/class/cdc/cdc_rndis_host.c',
    'src/class/dfu/dfu_device.c',
    'src/class/dfu/dfu_rt_device.c',
    'src/class/hid/hid_device.c',
    'src/class/hid/hid_host.c',
    'src/class/midi/midi_device.c',
    'src/class/msc/msc_device.c',
    'src/class/msc/msc_host.c',
    'src/class/net/ecm_rndis_device.c',
    'src/class/net/ncm_device.c',
    'src/class/usbtmc/usbtmc_device.c',
    'src/class/vendor/vendor_device.c',
    'src/class/vendor/vendor_host.c',
    'src/class/video/video_device.c',
    'src/common/tusb_fifo.c',
    'src/device/usbd_control.c',
    'src/device/usbd.c',
    'src/host/hub.c',
    'src/host/usbh.c',
    'src/tusb.c'
])

if tusb_mcu in ['OPT_MCU_NONE']
    tusb_src += files([
        'src/portable/template/dcd_template.c',
    ])
elif tusb_mcu in ['OPT_MCU_FT90X', 'OPT_MCU_FT93X']
    tusb_src += files([
        'src/portable/bridgetek/ft9xx/dcd_ft9xx.c',
    ])
elif tusb_mcu in ['OPT_MCU_BCM2711']
    tusb_src += files([
        'src/portable/broadcom/synopsys/dcd_synopsys.c',
    ])
elif tusb_mcu in ['OPT_MCU_MIMXRT10XX', 'OPT_MCU_LPC18XX', 'OPT_MCU_LPC43XX']
    tusb_src += files([
        'src/portable/chipidea/ci_hs/dcd_ci_hs.c',
        'src/portable/chipidea/ci_hs/hcd_ci_hs.c',
        'src/portable/ehci/ehci.c',
    ])
elif tusb_mcu in ['OPT_MCU_DA1469X']
    tusb_src += files([
        'src/portable/dialog/da146xx/dcd_da146xx.c',
    ])
elif tusb_mcu in ['OPT_MCU_ESP32S2', 'OPT_MCU_ESP32S3']
    tusb_src += files([
        'src/portable/espressif/esp32sx/dcd_esp32sx.c',
    ])
elif tusb_mcu in ['OPT_MCU_MSP432E4', 'OPT_MCU_TM4C123', 'OPT_MCU_TM4C129']
    tusb_src += files([
        'src/portable/mentor/musb/dcd_musb.c',
        'src/portable/mentor/musb/hcd_musb.c',
    ])
elif tusb_mcu in ['OPT_MCU_PIC32MZ']
    tusb_src += files([
        'src/portable/microchip/pic32mz/dcd_pic32mz.c',
    ])
elif tusb_mcu in ['OPT_MCU_SAMD11', 'OPT_MCU_SAMD21', 'OPT_MCU_SAMD51OPT_MCU_SAME5X', 'OPT_MCU_SAML22', 'OPT_MCU_SAML21']
    tusb_src += files([
        'src/portable/microchip/samd/dcd_samd.c',
    ])
elif tusb_mcu in ['OPT_MCU_SAMG']
    tusb_src += files([
        'src/portable/microchip/samg/dcd_samg.c',
    ])
elif tusb_mcu in ['OPT_MCU_SAMX7X']
    tusb_src += files([
        'src/portable/microchip/samx7x/dcd_samx7x.c',
    ])
elif tusb_mcu in ['OPT_MCU_MM32F327X']
    tusb_src += files([
        'src/portable/mindmotion/mm32/dcd_mm32f327x_otg.c',
    ])
elif tusb_mcu in ['OPT_MCU_NRF5X']
    tusb_src += files([
        'src/portable/nordic/nrf5x/dcd_nrf5x.c',
    ])
elif tusb_mcu in ['OPT_MCU_NUC120']
    tusb_src += files([
        'src/portable/nuvoton/nuc120/dcd_nuc120.c',
    ])
elif tusb_mcu in ['OPT_MCU_NUC121', 'OPT_MCU_NUC126']
    tusb_src += files([
        'src/portable/nuvoton/nuc121/dcd_nuc121.c',
    ])
elif tusb_mcu in ['OPT_MCU_NUC505']
    tusb_src += files([
        'src/portable/nuvoton/nuc505/dcd_nuc505.c',
    ])
elif tusb_mcu in ['OPT_MCU_MKL25ZXX', 'OPT_MCU_K32L2BXX']
    tusb_src += files([
        'src/portable/nxp/khci/dcd_khci.c',
        'src/portable/nxp/khci/hcd_khci.c',
    ])
elif tusb_mcu in ['OPT_MCU_LPC11UXX', 'OPT_MCU_LPC13XX', 'OPT_MCU_LPC15XX']
    tusb_src += files([
        'src/portable/nxp/lpc_ip3511/dcd_lpc_ip3511.c',
    ])
elif tusb_mcu in ['OPT_MCU_LPC175X_6X', 'OPT_MCU_LPC177X_8X', 'OPT_MCU_LPC40XX']
    tusb_src += files([
        'src/portable/nxp/lpc17_40/dcd_lpc17_40.c',
        'src/portable/nxp/lpc17_40/hcd_lpc17_40.c',
        'src/portable/ohci/ohci.c',
    ])
elif tusb_mcu in ['OPT_MCU_RP2040']
    tusb_src += files([
        'src/portable/raspberrypi/rp2040/dcd_rp2040.c',
        'src/portable/raspberrypi/rp2040/hcd_rp2040.c',
        'src/portable/raspberrypi/rp2040/rp2040_usb.c',
    ])
elif tusb_mcu in ['OPT_MCU_RX63X', 'OPT_MCU_RX65X', 'OPT_MCU_RX72N']
    tusb_src += files([
        'src/portable/renesas/usba/dcd_usba.c',
        'src/portable/renesas/usba/hcd_usba.c',
    ])
elif tusb_mcu in ['OPT_MCU_CXD56']
    tusb_src += files([
        'src/portable/sony/cxd56/dcd_cxd56.c',
    ])
elif tusb_mcu in ['OPT_MCU_STM32F0', 'OPT_MCU_STM32F3', 'OPT_MCU_STM32L0', 'OPT_MCU_STM32L1', 'OPT_MCU_STM32G4', 'OPT_MCU_STM32WB', 'OPT_MCU_STM32F1']
    tusb_src += files([
        'src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c',
    ])
# elif tusb_mcu in ['OPT_MCU_STM32F1', 'OPT_MCU_STM32F2', 'OPT_MCU_STM32F4', 'OPT_MCU_STM32F7', 'OPT_MCU_STM32H7', 'OPT_MCU_STM32L4', 'OPT_MCU_GD32VF103']
# 	tusb_src += files([
# 		'src/portable/st/synopsys/dcd_synopsys.c',
# 	])
elif tusb_mcu in ['OPT_MCU_STM32F1', 'OPT_MCU_STM32F2', 'OPT_MCU_STM32F4', 'OPT_MCU_STM32F7', 'OPT_MCU_STM32H7', 'OPT_MCU_STM32L4',
                  'OPT_MCU_ESP32S2', 'OPT_MCU_ESP32S3', 'OPT_MCU_GD32VF103', 'OPT_MCU_BCM2711', 'OPT_MCU_BCM2835', 'OPT_MCU_BCM2837', 'OPT_MCU_EFM32GG', 'OPT_MCU_XMC4000']
    tusb_src += files([
        'src/portable/synopsys/dwc2/dcd_dwc2.c',
    ])
elif tusb_mcu in ['OPT_MCU_F1C100S']
    tusb_src += files([
        'src/portable/sunxi/dcd_sunxi_musb.c',
    ])
elif tusb_mcu in ['OPT_MCU_MSP430x5xx']
    tusb_src += files([
        'src/portable/ti/msp430x5xx/dcd_msp430x5xx.c',
    ])
elif tusb_mcu in ['OPT_MCU_VALENTYUSB_EPTRI']
    tusb_src += files([
        'src/portable/valentyusb/eptri/dcd_eptri.c',
    ])
else
    error('Unsupported tusb_mcu family ' + tusb_mcu)
endif

tusb_dep = declare_dependency(
    include_directories : tusb_inc,
    compile_args : [f'-DCFG_TUSB_MCU=@tusb_mcu@'],
    sources : tusb_src,
    version : tusb_ver
)
