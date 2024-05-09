************
Dependencies
************

MCU low-level peripheral driver and external libraries for building TinyUSB examples

========================================  ==============================================================  ========================================  =======================================================================================================================================================================================================
Local Path                                Repo                                                            Commit                                    Required by
========================================  ==============================================================  ========================================  =======================================================================================================================================================================================================
hw/mcu/allwinner                          https://github.com/hathach/allwinner_driver.git                 8e5e89e8e132c0fd90e72d5422e5d3d68232b756  fc100s
hw/mcu/bridgetek/ft9xx/ft90x-sdk          https://github.com/BRTSG-FOSS/ft90x-sdk.git                     91060164afe239fcb394122e8bf9eb24d3194eb1  brtmm90x
hw/mcu/broadcom                           https://github.com/adafruit/broadcom-peripherals.git            08370086080759ed54ac1136d62d2ad24c6fa267  broadcom_32bit broadcom_64bit
hw/mcu/gd/nuclei-sdk                      https://github.com/Nuclei-Software/nuclei-sdk.git               7eb7bfa9ea4fbeacfafe1d5f77d5a0e6ed3922e7  gd32vf103
hw/mcu/infineon/mtb-xmclib-cat3           https://github.com/Infineon/mtb-xmclib-cat3.git                 daf5500d03cba23e68c2f241c30af79cd9d63880  xmc4000
hw/mcu/microchip                          https://github.com/hathach/microchip_driver.git                 9e8b37e307d8404033bb881623a113931e1edf27  sam3x samd11 samd21 samd51 same5x same7x saml2x samg
hw/mcu/mindmotion/mm32sdk                 https://github.com/hathach/mm32sdk.git                          0b79559eb411149d36e073c1635c620e576308d4  mm32
hw/mcu/nordic/nrfx                        https://github.com/NordicSemiconductor/nrfx.git                 2527e3c8449cfd38aee41598e8af8492f410ed15  nrf
hw/mcu/nuvoton                            https://github.com/majbthrd/nuc_driver.git                      2204191ec76283371419fbcec207da02e1bc22fa  nuc
hw/mcu/nxp/lpcopen                        https://github.com/hathach/nxp_lpcopen.git                      84e0bd3e43910aaf71eefd62075cf57495418312  lpc11 lpc13 lpc15 lpc17 lpc18 lpc40 lpc43
hw/mcu/nxp/mcux-sdk                       https://github.com/hathach/mcux-sdk.git                         950819b7de9b32f92c3edf396bc5ffb8d66e7009  kinetis_k32l2 kinetis_kl lpc51 lpc54 lpc55 mcx imxrt
hw/mcu/raspberry_pi/Pico-PIO-USB          https://github.com/sekigon-gonnoc/Pico-PIO-USB.git              d00a10a8c425d0d40f81b87169102944b01f3bb3  rp2040
hw/mcu/renesas/fsp                        https://github.com/renesas/fsp.git                              d52e5a6a59b7c638da860c2bb309b6e78e752ff8  ra
hw/mcu/renesas/rx                         https://github.com/kkitayam/rx_device.git                       706b4e0cf485605c32351e2f90f5698267996023  rx
hw/mcu/silabs/cmsis-dfp-efm32gg12b        https://github.com/cmsis-packs/cmsis-dfp-efm32gg12b.git         f1c31b7887669cb230b3ea63f9b56769078960bc  efm32
hw/mcu/sony/cxd56/spresense-exported-sdk  https://github.com/sonydevworld/spresense-exported-sdk.git      2ec2a1538362696118dc3fdf56f33dacaf8f4067  spresense
hw/mcu/st/cmsis_device_f0                 https://github.com/STMicroelectronics/cmsis_device_f0.git       2fc25ee22264bc27034358be0bd400b893ef837e  stm32f0
hw/mcu/st/cmsis_device_f1                 https://github.com/STMicroelectronics/cmsis_device_f1.git       6601104a6397299b7304fd5bcd9a491f56cb23a6  stm32f1
hw/mcu/st/cmsis_device_f2                 https://github.com/STMicroelectronics/cmsis_device_f2.git       182fcb3681ce116816feb41b7764f1b019ce796f  stm32f2
hw/mcu/st/cmsis_device_f3                 https://github.com/STMicroelectronics/cmsis_device_f3.git       5e4ee5ed7a7b6c85176bb70a9fd3c72d6eb99f1b  stm32f3
hw/mcu/st/cmsis_device_f4                 https://github.com/STMicroelectronics/cmsis_device_f4.git       2615e866fa48fe1ff1af9e31c348813f2b19e7ec  stm32f4
hw/mcu/st/cmsis_device_f7                 https://github.com/STMicroelectronics/cmsis_device_f7.git       fc676ef1ad177eb874eaa06444d3d75395fc51f4  stm32f7
hw/mcu/st/cmsis_device_g0                 https://github.com/STMicroelectronics/cmsis_device_g0.git       3a23e1224417f3f2d00300ecd620495e363f2094  stm32g0
hw/mcu/st/cmsis_device_g4                 https://github.com/STMicroelectronics/cmsis_device_g4.git       ce822adb1dc552b3aedd13621edbc7fdae124878  stm32g4
hw/mcu/st/cmsis_device_h7                 https://github.com/STMicroelectronics/cmsis_device_h7.git       60dc2c913203dc8629dc233d4384dcc41c91e77f  stm32h7
hw/mcu/st/cmsis_device_l0                 https://github.com/STMicroelectronics/cmsis_device_l0.git       06748ca1f93827befdb8b794402320d94d02004f  stm32l0
hw/mcu/st/cmsis_device_l1                 https://github.com/STMicroelectronics/cmsis_device_l1.git       7f16ec0a1c4c063f84160b4cc6bf88ad554a823e  stm32l1
hw/mcu/st/cmsis_device_l4                 https://github.com/STMicroelectronics/cmsis_device_l4.git       6ca7312fa6a5a460b5a5a63d66da527fdd8359a6  stm32l4
hw/mcu/st/cmsis_device_l5                 https://github.com/STMicroelectronics/cmsis_device_l5.git       d922865fc0326a102c26211c44b8e42f52c1e53d  stm32l5
hw/mcu/st/cmsis_device_u5                 https://github.com/STMicroelectronics/cmsis_device_u5.git       06d7edade7167b0eafdd550bf77cfc4fa98eae2e  stm32u5
hw/mcu/st/cmsis_device_wb                 https://github.com/STMicroelectronics/cmsis_device_wb.git       9c5d1920dd9fabbe2548e10561d63db829bb744f  stm32wb
hw/mcu/st/stm32f0xx_hal_driver            https://github.com/STMicroelectronics/stm32f0xx_hal_driver.git  0e95cd88657030f640a11e690a8a5186c7712ea5  stm32f0
hw/mcu/st/stm32f1xx_hal_driver            https://github.com/STMicroelectronics/stm32f1xx_hal_driver.git  1dd9d3662fb7eb2a7f7d3bc0a4c1dc7537915a29  stm32f1
hw/mcu/st/stm32f2xx_hal_driver            https://github.com/STMicroelectronics/stm32f2xx_hal_driver.git  c75ace9b908a9aca631193ebf2466963b8ea33d0  stm32f2
hw/mcu/st/stm32f3xx_hal_driver            https://github.com/STMicroelectronics/stm32f3xx_hal_driver.git  1761b6207318ede021706e75aae78f452d72b6fa  stm32f3
hw/mcu/st/stm32f4xx_hal_driver            https://github.com/STMicroelectronics/stm32f4xx_hal_driver.git  04e99fbdabd00ab8f370f377c66b0a4570365b58  stm32f4
hw/mcu/st/stm32f7xx_hal_driver            https://github.com/STMicroelectronics/stm32f7xx_hal_driver.git  f7ffdf6bf72110e58b42c632b0a051df5997e4ee  stm32f7
hw/mcu/st/stm32g0xx_hal_driver            https://github.com/STMicroelectronics/stm32g0xx_hal_driver.git  e911b12c7f67084d7f6b76157a4c0d4e2ec3779c  stm32g0
hw/mcu/st/stm32g4xx_hal_driver            https://github.com/STMicroelectronics/stm32g4xx_hal_driver.git  8b4518417706d42eef5c14e56a650005abf478a8  stm32g4
hw/mcu/st/stm32h7xx_hal_driver            https://github.com/STMicroelectronics/stm32h7xx_hal_driver.git  d8461b980b59b1625207d8c4f2ce0a9c2a7a3b04  stm32h7
hw/mcu/st/stm32l0xx_hal_driver            https://github.com/STMicroelectronics/stm32l0xx_hal_driver.git  fbdacaf6f8c82a4e1eb9bd74ba650b491e97e17b  stm32l0
hw/mcu/st/stm32l1xx_hal_driver            https://github.com/STMicroelectronics/stm32l1xx_hal_driver.git  44efc446fa69ed8344e7fd966e68ed11043b35d9  stm32l1
hw/mcu/st/stm32l4xx_hal_driver            https://github.com/STMicroelectronics/stm32l4xx_hal_driver.git  aee3d5bf283ae5df87532b781bdd01b7caf256fc  stm32l4
hw/mcu/st/stm32l5xx_hal_driver            https://github.com/STMicroelectronics/stm32l5xx_hal_driver.git  675c32a75df37f39d50d61f51cb0dcf53f07e1cb  stm32l5
hw/mcu/st/stm32u5xx_hal_driver            https://github.com/STMicroelectronics/stm32u5xx_hal_driver.git  4d93097a67928e9377e655ddd14622adc31b9770  stm32u5
hw/mcu/st/stm32wbxx_hal_driver            https://github.com/STMicroelectronics/stm32wbxx_hal_driver.git  2c5f06638be516c1b772f768456ba637f077bac8  stm32wb
hw/mcu/ti                                 https://github.com/hathach/ti_driver.git                        143ed6cc20a7615d042b03b21e070197d473e6e5  msp430 msp432e4 tm4c123
hw/mcu/wch/ch32f20x                       https://github.com/openwch/ch32f20x.git                         77c4095087e5ed2c548ec9058e655d0b8757663b  ch32f20x
hw/mcu/wch/ch32v307                       https://github.com/openwch/ch32v307.git                         17761f5cf9dbbf2dcf665b7c04934188add20082  ch32v307
lib/CMSIS_5                               https://github.com/ARM-software/CMSIS_5.git                     20285262657d1b482d132d20d755c8c330d55c1f  imxrt kinetis_k32l2 kinetis_kl lpc51 lpc54 lpc55 mcx mm32 msp432e4 nrf ra saml2xstm32f0 stm32f1 stm32f2 stm32f3 stm32f4 stm32f7 stm32g0 stm32g4 stm32h7 stm32l0 stm32l1 stm32l4 stm32l5 stm32u5 stm32wb
lib/FreeRTOS-Kernel                       https://github.com/FreeRTOS/FreeRTOS-Kernel.git                 4ff01a7a4a51f53b44496aefee1e3c0071b7b173  all
lib/lwip                                  https://github.com/lwip-tcpip/lwip.git                          159e31b689577dbf69cf0683bbaffbd71fa5ee10  all
lib/sct_neopixel                          https://github.com/gsteiert/sct_neopixel.git                    e73e04ca63495672d955f9268e003cffe168fcd8  lpc55
tools/uf2                                 https://github.com/microsoft/uf2.git                            19615407727073e36d81bf239c52108ba92e7660  all
========================================  ==============================================================  ========================================  =======================================================================================================================================================================================================
