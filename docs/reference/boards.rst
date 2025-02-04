****************
Supported Boards
****************

The board support code is only used for self-contained examples and testing. It is not used when TinyUSB is part of a larger project.
It is responsible for getting the MCU started and the USB peripheral clocked with minimal of on-board devices

-  One LED : for status
-  One Button : to get input from user
-  One UART : optional for device, but required for host examples

Following boards are supported

Analog Devices
--------------

=============  ================  ========  ===========================================================================================================================  ======
Board          Name              Family    URL                                                                                                                          Note
=============  ================  ========  ===========================================================================================================================  ======
max32650evkit  MAX32650 EVKIT    max32650  https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/max32650-evkit.html#eb-overview
max32650fthr   MAX32650 Feather  max32650  https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/max32650fthr.html
max32651evkit  MAX32651 EVKIT    max32650  https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/max32651-evkit.html
max32666evkit  MAX32666 EVKIT    max32666  https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/max32666evkit.html
max32666fthr   MAX32666 Feather  max32666  https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/max32666fthr.html
apard32690     APARD32690-SL     max32690  https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/ad-apard32690-sl.html
max32690evkit  MAX32690 EVKIT    max32690  https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/max32690evkit.html
max78002evkit  MAX78002 EVKIT    max78002  https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/max78002evkit.html
=============  ================  ========  ===========================================================================================================================  ======

Bridgetek
---------

=========  =========  ========  =====================================  ======
Board      Name       Family    URL                                    Note
=========  =========  ========  =====================================  ======
mm900evxb  MM900EVxB  brtmm90x  https://brtchip.com/product/mm900ev1b
=========  =========  ========  =====================================  ======

Espressif
---------

=========================  ==============================  =========  ========================================================================================================  ======
Board                      Name                            Family     URL                                                                                                       Note
=========================  ==============================  =========  ========================================================================================================  ======
adafruit_feather_esp32_v2  Adafruit Feather ESP32 v2       espressif  https://www.adafruit.com/product/5400
adafruit_feather_esp32s2   Adafruit Feather ESP32S2        espressif  https://www.adafruit.com/product/5000
adafruit_feather_esp32s3   Adafruit Feather ESP32S3        espressif  https://www.adafruit.com/product/5323
adafruit_magtag_29gray     Adafruit MagTag 2.9" Grayscale  espressif  https://www.adafruit.com/product/4800
adafruit_metro_esp32s2     Adafruit Metro ESP32-S2         espressif  https://www.adafruit.com/product/4775
espressif_addax_1          Espresif Addax-1                espressif  n/a
espressif_c3_devkitc       Espresif C3 DevKitC             espressif  https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c3/esp32-c3-devkitc-02/index.html
espressif_c6_devkitc       Espresif C6 DevKitC             espressif  https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c6/esp32-c6-devkitc-1/index.html
espressif_kaluga_1         Espresif Kaluga 1               espressif  https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s2/esp32-s2-kaluga-1/index.html
espressif_p4_function_ev   Espresif P4 Function EV         espressif  https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html
espressif_s2_devkitc       Espresif S2 DevKitC             espressif  https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s2/esp32-s2-devkitc-1/index.html
espressif_s3_devkitc       Espresif S3 DevKitC             espressif  https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/index.html
espressif_s3_devkitm       Espresif S3 DevKitM             espressif  https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitm-1/index.html
espressif_saola_1          Espresif S2 Saola 1             espressif  https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s2/esp32-s2-saola-1/index.html
=========================  ==============================  =========  ========================================================================================================  ======

GigaDevice
----------

==================  ==================  =========  =============================  ======
Board               Name                Family     URL                            Note
==================  ==================  =========  =============================  ======
sipeed_longan_nano  Sipeed Longan Nano  gd32vf103  https://longan.sipeed.com/en/
==================  ==================  =========  =============================  ======

Infineon
--------

=============  =================  ========  =============================================================================  ======
Board          Name               Family    URL                                                                            Note
=============  =================  ========  =============================================================================  ======
xmc4500_relax  XMC4500 relax kit  xmc4000   https://www.infineon.com/cms/en/product/evaluation-boards/kit_xmc45_relax_v1/
xmc4700_relax  XMC4700 relax kit  xmc4000   https://www.infineon.com/cms/en/product/evaluation-boards/kit_xmc47_relax_v1/
=============  =================  ========  =============================================================================  ======

Microchip
---------

=========================  ===================================  ==========  =================================================================================  ======
Board                      Name                                 Family      URL                                                                                Note
=========================  ===================================  ==========  =================================================================================  ======
olimex_emz64               Olimex PIC32-EMZ64                   pic32mz     https://www.olimex.com/Products/PIC/Development/PIC32-EMZ64/open-source-hardware
olimex_hmz144              Olimex PIC32-HMZ144                  pic32mz     https://www.olimex.com/Products/PIC/Development/PIC32-HMZ144/open-source-hardware
cynthion_d11               Great Scott Gadgets Cynthion         samd11      https://greatscottgadgets.com/cynthion/
samd11_xplained            SAMD11 Xplained Pro                  samd11      https://www.microchip.com/en-us/development-tool/ATSAMD11-XPRO
atsamd21_xpro              SAMD21 Xplained Pro                  samd21      https://www.microchip.com/DevelopmentTools/ProductDetails/ATSAMD21-XPRO
circuitplayground_express  Adafruit Circuit Playground Express  samd21      https://www.adafruit.com/product/3333
curiosity_nano             SAMD21 Curiosty Nano                 samd21      https://www.microchip.com/en-us/development-tool/dm320119
cynthion_d21               Great Scott Gadgets Cynthion         samd21      https://greatscottgadgets.com/cynthion/
feather_m0_express         Adafruit Feather M0 Express          samd21      https://www.adafruit.com/product/3403
itsybitsy_m0               Adafruit ItsyBitsy M0                samd21      https://www.adafruit.com/product/3727
metro_m0_express           Adafruit Metro M0 Express            samd21      https://www.adafruit.com/product/3505
qtpy                       Adafruit QT Py                       samd21      https://www.adafruit.com/product/4600
seeeduino_xiao             Seeeduino XIAO                       samd21      https://wiki.seeedstudio.com/Seeeduino-XIAO/
sparkfun_samd21_mini_usb   SparkFun SAMD21 Mini                 samd21      https://www.sparkfun.com/products/13664
trinket_m0                 Adafruit Trinket M0                  samd21      https://www.adafruit.com/product/3500
d5035_01                   D5035-01                             samd5x_e5x  https://github.com/RudolphRiedel/USB_CAN-FD
feather_m4_express         Adafruit Feather M4 Express          samd5x_e5x  https://www.adafruit.com/product/3857
itsybitsy_m4               Adafruit ItsyBitsy M4                samd5x_e5x  https://www.adafruit.com/product/3800
metro_m4_express           Adafruit Metro M4 Express            samd5x_e5x  https://www.adafruit.com/product/3382
pybadge                    Adafruit PyBadge                     samd5x_e5x  https://www.adafruit.com/product/4200
pyportal                   Adafruit PyPortal                    samd5x_e5x  https://www.adafruit.com/product/4116
same54_xplained            SAME54 Xplained Pro                  samd5x_e5x  https://www.microchip.com/DevelopmentTools/ProductDetails/ATSAME54-XPRO
samg55_xplained            SAMG55 Xplained Pro                  samg        https://www.microchip.com/DevelopmentTools/ProductDetails/ATSAMG55-XPRO
atsaml21_xpro              SAML21 Xplained Pro                  saml2x      https://www.microchip.com/en-us/development-tool/atsaml21-xpro-b
saml22_feather             SAML22 Feather                       saml2x      https://github.com/joeycastillo/Feather-Projects/tree/main/SAML22%20Feather
sensorwatch_m0             SensorWatch                          saml2x      https://github.com/joeycastillo/Sensor-Watch
=========================  ===================================  ==========  =================================================================================  ======

MindMotion
----------

=====================  ======================================  ========  ===============================================================================================  ======
Board                  Name                                    Family    URL                                                                                              Note
=====================  ======================================  ========  ===============================================================================================  ======
mm32f327x_mb39         MM32F3273G9P MB-039                     mm32      https://www.mindmotion.com.cn/support/development_tools/evaluation_boards/evboard/mm32f3273g9p/
mm32f327x_pitaya_lite  DshanMCU Pitaya Lite with MM32F3273G8P  mm32      https://gitee.com/weidongshan/DshanMCU-Pitaya-c
=====================  ======================================  ========  ===============================================================================================  ======

NXP
---

==================  =========================================  =============  =========================================================================================================================================================================  ======
Board               Name                                       Family         URL                                                                                                                                                                        Note
==================  =========================================  =============  =========================================================================================================================================================================  ======
metro_m7_1011       Adafruit Metro M7 1011                     imxrt          https://www.adafruit.com/product/5600
metro_m7_1011_sd    Adafruit Metro M7 1011 SD                  imxrt          https://www.adafruit.com/product/5600
mimxrt1010_evk      i.MX RT1010 Evaluation Kit                 imxrt          https://www.nxp.com/design/design-center/development-boards-and-designs/i-mx-evaluation-and-development-boards/i-mx-rt1010-evaluation-kit:MIMXRT1010-EVK
mimxrt1015_evk      i.MX RT1015 Evaluation Kit                 imxrt          https://www.nxp.com/design/design-center/development-boards-and-designs/MIMXRT1015-EVK
mimxrt1020_evk      i.MX RT1020 Evaluation Kit                 imxrt          https://www.nxp.com/design/design-center/development-boards-and-designs/MIMXRT1020-EVK
mimxrt1024_evk      i.MX RT1024 Evaluation Kit                 imxrt          https://www.nxp.com/design/design-center/development-boards-and-designs/i-mx-evaluation-and-development-boards/i-mx-rt1024-evaluation-kit:MIMXRT1024-EVK
mimxrt1050_evkb     i.MX RT1050 Evaluation Kit revB            imxrt          https://www.nxp.com/part/IMXRT1050-EVKB
mimxrt1060_evk      i.MX RT1060 Evaluation Kit revB            imxrt          https://www.nxp.com/design/design-center/development-boards-and-designs/MIMXRT1060-EVKB
mimxrt1064_evk      i.MX RT1064 Evaluation Kit                 imxrt          https://www.nxp.com/design/design-center/development-boards-and-designs/MIMXRT1064-EVK
mimxrt1170_evkb     i.MX RT1070 Evaluation Kit                 imxrt          https://www.nxp.com/design/design-center/development-boards-and-designs/i-mx-evaluation-and-development-boards/i-mx-rt1170-evaluation-kit:MIMXRT1170-EVKB
teensy_40           Teensy 4.0                                 imxrt          https://www.pjrc.com/store/teensy40.html
teensy_41           Teensy 4.1                                 imxrt          https://www.pjrc.com/store/teensy41.html
frdm_k64f           Freedom K64F                               kinetis_k      https://www.nxp.com/design/design-center/development-boards-and-designs/general-purpose-mcus/freedom-development-platform-for-kinetis-k64-k63-and-k24-mcus:FRDM-K64F
teensy_35           Teensy 3.5                                 kinetis_k      https://www.pjrc.com/store/teensy35.html
frdm_k32l2a4s       Freedom K32L2A4S                           kinetis_k32l2  https://www.nxp.com/design/design-center/development-boards-and-designs/FRDM-K32L2A4S
frdm_k32l2b         Freedom K32L2B3                            kinetis_k32l2  https://www.nxp.com/design/design-center/development-boards-and-designs/general-purpose-mcus/nxp-freedom-development-platform-for-k32-l2b-mcus:FRDM-K32L2B3
kuiic               Kuiic                                      kinetis_k32l2  https://github.com/nxf58843/kuiic
frdm_kl25z          fomu                                       kinetis_kl     https://www.nxp.com/design/design-center/development-boards-and-designs/general-purpose-mcus/freedom-development-platform-for-kinetis-kl14-kl15-kl24-kl25-mcus:FRDM-KL25Z
lpcxpresso11u37     LPCXpresso11U37                            lpc11          https://www.nxp.com/design/design-center/development-boards-and-designs/OM13074
lpcxpresso11u68     LPCXpresso11U68                            lpc11          https://www.nxp.com/design/design-center/development-boards-and-designs/OM13058
lpcxpresso1347      LPCXpresso1347                             lpc13          https://www.nxp.com/products/no-longer-manufactured/lpcxpresso-board-for-lpc1347:OM13045
lpcxpresso1549      LPCXpresso1549                             lpc15          https://www.nxp.com/design/design-center/development-boards-and-designs/OM13056
lpcxpresso1769      LPCXpresso1769                             lpc17          https://www.nxp.com/design/design-center/development-boards-and-designs/OM13000
mbed1768            mbed 1768                                  lpc17          https://www.nxp.com/products/processors-and-microcontrollers/arm-microcontrollers/general-purpose-mcus/lpc1700-arm-cortex-m3/arm-mbed-lpc1768-board:OM11043
lpcxpresso18s37     LPCXpresso18s37                            lpc18          https://www.nxp.com/design/design-center/software/development-software/mcuxpresso-software-and-tools-/lpcxpresso-boards/lpcxpresso18s37-development-board:OM13076
mcb1800             Keil MCB1800                               lpc18          https://www.keil.com/arm/mcb1800/
ea4088_quickstart   Embedded Artists LPC4088 QuickStart Board  lpc40          https://www.embeddedartists.com/products/lpc4088-quickstart-board/
ea4357              Embedded Artists LPC4357 Development Kit   lpc43          https://www.embeddedartists.com/products/lpc4357-developers-kit/
lpcxpresso43s67     LPCXpresso43S67                            lpc43          https://www.nxp.com/design/design-center/software/development-software/mcuxpresso-software-and-tools-/lpcxpresso-boards/lpcxpresso43s67-development-board:OM13084
lpcxpresso51u68     LPCXpresso51u68                            lpc51          https://www.nxp.com/products/processors-and-microcontrollers/arm-microcontrollers/general-purpose-mcus/lpcxpresso51u68-for-the-lpc51u68-mcus:OM40005
lpcxpresso54114     LPCXpresso54114                            lpc54          https://www.nxp.com/design/design-center/software/development-software/mcuxpresso-software-and-tools-/lpcxpresso-boards/lpcxpresso54114-board:OM13089
lpcxpresso54608     LPCXpresso54608                            lpc54          https://www.nxp.com/design/design-center/software/development-software/mcuxpresso-software-and-tools-/lpcxpresso-development-board-for-lpc5460x-mcus:OM13092
lpcxpresso54628     LPCXpresso54628                            lpc54          https://www.nxp.com/design/design-center/software/development-software/mcuxpresso-software-and-tools-/lpcxpresso-boards/lpcxpresso54628-development-board:OM13098
double_m33_express  Double M33 Express                         lpc55          https://www.crowdsupply.com/steiert-solutions/double-m33-express
lpcxpresso55s28     LPCXpresso55s28                            lpc55          https://www.nxp.com/design/design-center/software/development-software/mcuxpresso-software-and-tools-/lpcxpresso-boards/lpcxpresso55s28-development-board:LPC55S28-EVK
lpcxpresso55s69     LPCXpresso55s69                            lpc55          https://www.nxp.com/design/design-center/software/development-software/mcuxpresso-software-and-tools-/lpcxpresso-boards/lpcxpresso55s69-development-board:LPC55S69-EVK
mcu_link            MCU Link                                   lpc55          https://www.nxp.com/design/design-center/software/development-software/mcuxpresso-software-and-tools-/mcu-link-debug-probe:MCU-LINK
frdm_mcxa153        Freedom MCXA153                            mcx            https://www.nxp.com/design/design-center/development-boards-and-designs/FRDM-MCXA153
frdm_mcxn947        Freedom MCXN947                            mcx            https://www.nxp.com/design/design-center/development-boards-and-designs/FRDM-MCXN947
mcxn947brk          MCXN947 Breakout                           mcx            n/a
==================  =========================================  =============  =========================================================================================================================================================================  ======

Nordic Semiconductor
--------------------

===========================  =====================================  ========  ==============================================================================  ======
Board                        Name                                   Family    URL                                                                             Note
===========================  =====================================  ========  ==============================================================================  ======
adafruit_clue                Adafruit CLUE                          nrf       https://www.adafruit.com/product/4500
arduino_nano33_ble           Arduino Nano 33 BLE                    nrf       https://store.arduino.cc/arduino-nano-33-ble
circuitplayground_bluefruit  Adafruit Circuit Playground Bluefruit  nrf       https://www.adafruit.com/product/4333
feather_nrf52840_express     Adafruit Feather nRF52840 Express      nrf       https://www.adafruit.com/product/4062
feather_nrf52840_sense       Adafruit Feather nRF52840 Sense        nrf       https://www.adafruit.com/product/4516
itsybitsy_nrf52840           Adafruit ItsyBitsy nRF52840 Express    nrf       https://www.adafruit.com/product/4481
pca10056                     Nordic nRF52840DK                      nrf       https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF52840-DK
pca10059                     Nordic nRF52840 Dongle                 nrf       https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF52840-Dongle
pca10095                     Nordic nRF5340 DK                      nrf       https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF5340-DK
pca10100                     Nordic nRF52833 DK                     nrf       https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF52833-DK
===========================  =====================================  ========  ==============================================================================  ======

Raspberry Pi
------------

=================  =================  ==============  ==========================================================  ======
Board              Name               Family          URL                                                         Note
=================  =================  ==============  ==========================================================  ======
raspberrypi_zero   Raspberry Pi Zero  broadcom_32bit  https://www.raspberrypi.org/products/raspberry-pi-zero/
raspberrypi_cm4    Raspberry CM4      broadcom_64bit  https://www.raspberrypi.org/products/compute-module-4
raspberrypi_zero2  Raspberry Zero2    broadcom_64bit  https://www.raspberrypi.org/products/raspberry-pi-zero-2-w
=================  =================  ==============  ==========================================================  ======

Renesas
-------

==============  ===========================  ========  ================================================================================================================================================================  ======
Board           Name                         Family    URL                                                                                                                                                               Note
==============  ===========================  ========  ================================================================================================================================================================  ======
da14695_dk_usb  DA14695-00HQDEVKT-U          da1469x   https://www.renesas.com/en/products/wireless-connectivity/bluetooth-low-energy/da14695-00hqdevkt-u-smartbond-da14695-bluetooth-low-energy-52-usb-development-kit
da1469x_dk_pro  DA1469x Development Kit Pro  da1469x   https://lpccs-docs.renesas.com/um-b-090-da1469x_getting_started/DA1469x_The_hardware/DA1469x_The_hardware.html
portenta_c33    Arduino Portenta C33         ra        https://www.arduino.cc/pro/hardware-product-portenta-c33/
ra2a1_ek        RA2A1 EK                     ra        https://www.renesas.com/en/products/microcontrollers-microprocessors/ra-cortex-m-mcus/ek-ra2a1-evaluation-kit-ra2a1-mcu-group
ra4m1_ek        RA4M1 EK                     ra        https://www.renesas.com/en/products/microcontrollers-microprocessors/ra-cortex-m-mcus/ek-ra4m1-evaluation-kit-ra4m1-mcu-group
ra4m3_ek        RA4M3 EK                     ra        https://www.renesas.com/en/products/microcontrollers-microprocessors/ra-cortex-m-mcus/ek-ra4m3-evaluation-kit-ra4m3-mcu-group
ra6m1_ek        RA6M1 EK                     ra        https://www.renesas.com/en/products/microcontrollers-microprocessors/ra-cortex-m-mcus/ek-ra6m1-evaluation-kit-ra6m1-mcu-group
ra6m5_ek        RA6M5 EK                     ra        https://www.renesas.com/en/products/microcontrollers-microprocessors/ra-cortex-m-mcus/ek-ra6m5-evaluation-kit-ra6m5-mcu-group
ra8m1_ek        RA8M1 EK                     ra        https://www.renesas.com/en/products/microcontrollers-microprocessors/ra-cortex-m-mcus/ek-ra8m1-evaluation-kit-ra8m1-mcu-group
uno_r4          Arduino UNO R4               ra        https://store-usa.arduino.cc/pages/uno-r4
==============  ===========================  ========  ================================================================================================================================================================  ======

STMicroelectronics
------------------

===================  =================================  ========  =================================================================  ======
Board                Name                               Family    URL                                                                Note
===================  =================================  ========  =================================================================  ======
stm32c071nucleo      STM32C071 Nucleo                   stm32c0   https://www.st.com/en/evaluation-tools/nucleo-g071rb.html
stm32f070rbnucleo    STM32 F070 Nucleo                  stm32f0   https://www.st.com/en/evaluation-tools/nucleo-f070rb.html
stm32f072disco       STM32 F072 Discovery               stm32f0   https://www.st.com/en/evaluation-tools/32f072bdiscovery.html
stm32f072eval        STM32 F072 Eval                    stm32f0   https://www.st.com/en/evaluation-tools/stm32072b-eval.html
stm32f103_bluepill   STM32 F103 Bluepill                stm32f1   https://stm32-base.org/boards/STM32F103C8T6-Blue-Pill
stm32f103_mini_2     STM32 F103 Mini v2                 stm32f1   https://stm32-base.org/boards/STM32F103RCT6-STM32-Mini-V2.0
stm32f103ze_iar      IAR STM32 F103ze starter kit       stm32f1   n/a
stm32f207nucleo      STM32 F207 Nucleo                  stm32f2   https://www.st.com/en/evaluation-tools/nucleo-f207zg.html
stm32f303disco       STM32 F303 Discovery               stm32f3   https://www.st.com/en/evaluation-tools/stm32f3discovery.html
feather_stm32f405    Adafruit Feather STM32F405         stm32f4   https://www.adafruit.com/product/4382
pyboardv11           Pyboard v1.1                       stm32f4   https://www.adafruit.com/product/2390
stm32f401blackpill   STM32 F401 Blackpill               stm32f4   https://stm32-base.org/boards/STM32F401CCU6-WeAct-Black-Pill-V1.2
stm32f407blackvet    STM32 F407 Blackvet                stm32f4   https://stm32-base.org/boards/STM32F407VET6-STM32-F4VE-V2.0
stm32f407disco       STM32 F407 Discovery               stm32f4   https://www.st.com/en/evaluation-tools/stm32f4discovery.html
stm32f411blackpill   STM32 F411 Blackpill               stm32f4   https://stm32-base.org/boards/STM32F411CEU6-WeAct-Black-Pill-V2.0
stm32f411disco       STM32 F411 Discovery               stm32f4   https://www.st.com/en/evaluation-tools/32f411ediscovery.html
stm32f412disco       STM32 F412 Discovery               stm32f4   https://www.st.com/en/evaluation-tools/32f412gdiscovery.html
stm32f412nucleo      STM32 F412 Nucleo                  stm32f4   https://www.st.com/en/evaluation-tools/nucleo-f412zg.html
stm32f439nucleo      STM32 F439 Nucleo                  stm32f4   https://www.st.com/en/evaluation-tools/nucleo-f439zi.html
stlinkv3mini         Stlink-v3 mini                     stm32f7   https://www.st.com/en/development-tools/stlink-v3mini.html
stm32f723disco       STM32 F723 Discovery               stm32f7   https://www.st.com/en/evaluation-tools/32f723ediscovery.html
stm32f746disco       STM32 F746 Discovery               stm32f7   https://www.st.com/en/evaluation-tools/32f746gdiscovery.html
stm32f746nucleo      STM32 F746 Nucleo                  stm32f7   https://www.st.com/en/evaluation-tools/nucleo-f746zg.html
stm32f767nucleo      STM32 F767 Nucleo                  stm32f7   https://www.st.com/en/evaluation-tools/nucleo-f767zi.html
stm32f769disco       STM32 F769 Discovery               stm32f7   https://www.st.com/en/evaluation-tools/32f769idiscovery.html
stm32g0b1nucleo      STM32 G0B1 Nucleo                  stm32g0   https://www.st.com/en/evaluation-tools/nucleo-g0b1re.html
b_g474e_dpow1        STM32 B-G474E-DPOW1 Discovery kit  stm32g4   https://www.st.com/en/evaluation-tools/b-g474e-dpow1.html
stm32g474nucleo      STM32 G474 Nucleo                  stm32g4   https://www.st.com/en/evaluation-tools/nucleo-g474re.html
stm32g491nucleo      STM32 G491 Nucleo                  stm32g4   https://www.st.com/en/evaluation-tools/nucleo-g491re.html
stm32h503nucleo      STM32 H503 Nucleo                  stm32h5   https://www.st.com/en/evaluation-tools/nucleo-h503rb.html
stm32h563nucleo      STM32 H563 Nucleo                  stm32h5   https://www.st.com/en/evaluation-tools/nucleo-h563zi.html
stm32h573i_dk        STM32 H573i Discovery              stm32h5   https://www.st.com/en/evaluation-tools/stm32h573i-dk.html
daisyseed            Daisy Seed                         stm32h7   https://electro-smith.com/products/daisy-seed
stm32h723nucleo      STM32 H723 Nucleo                  stm32h7   https://www.st.com/en/evaluation-tools/nucleo-h723zg.html
stm32h743eval        STM32 H743 Eval                    stm32h7   https://www.st.com/en/evaluation-tools/stm32h743i-eval.html
stm32h743nucleo      STM32 H743 Nucleo                  stm32h7   https://www.st.com/en/evaluation-tools/nucleo-h743zi.html
stm32h745disco       STM32 H745 Discovery               stm32h7   https://www.st.com/en/evaluation-tools/stm32h745i-disco.html
stm32h750_weact      STM32 H750 WeAct                   stm32h7   https://www.adafruit.com/product/5032
stm32h750bdk         STM32 H750b Discovery Kit          stm32h7   https://www.st.com/en/evaluation-tools/stm32h750b-dk.html
waveshare_openh743i  Waveshare Open H743i               stm32h7   https://www.waveshare.com/openh743i-c-standard.htm
stm32l052dap52       STM32 L052 DAP                     stm32l0   n/a
stm32l0538disco      STM32 L0538 Discovery              stm32l0   https://www.st.com/en/evaluation-tools/32l0538discovery.html
stm32l412nucleo      STM32 L412 Nucleo                  stm32l4   https://www.st.com/en/evaluation-tools/nucleo-l412kb.html
stm32l476disco       STM32 L476 Disco                   stm32l4   https://www.st.com/en/evaluation-tools/32l476gdiscovery.html
stm32l4p5nucleo      STM32 L4P5 Nucleo                  stm32l4   https://www.st.com/en/evaluation-tools/nucleo-l4p5zg.html
stm32l4r5nucleo      STM32 L4R5 Nucleo                  stm32l4   https://www.st.com/en/evaluation-tools/nucleo-l4r5zi.html
b_u585i_iot2a        STM32 B-U585i IOT2A Discovery kit  stm32u5   https://www.st.com/en/evaluation-tools/b-u585i-iot02a.html
stm32u545nucleo      STM32 U545 Nucleo                  stm32u5   https://www.st.com/en/evaluation-tools/nucleo-u545re-q.html
stm32u575eval        STM32 U575 Eval                    stm32u5   https://www.st.com/en/evaluation-tools/stm32u575i-ev.html
stm32u575nucleo      STM32 U575 Nucleo                  stm32u5   https://www.st.com/en/evaluation-tools/nucleo-u575zi-q.html
stm32u5a5nucleo      STM32 U5a5 Nucleo                  stm32u5   https://www.st.com/en/evaluation-tools/nucleo-u5a5zj-q.html
stm32wb55nucleo      STM32 P-NUCLEO-WB55                stm32wb   https://www.st.com/en/evaluation-tools/p-nucleo-wb55.html
===================  =================================  ========  =================================================================  ======

Sunxi
-----

=======  =================  ========  =========================================  ======
Board    Name               Family    URL                                        Note
=======  =================  ========  =========================================  ======
f1c100s  Lctech Pi F1C200s  f1c100s   https://linux-sunxi.org/Lctech_Pi_F1C200s
=======  =================  ========  =========================================  ======

Texas Instruments
-----------------

=================  =====================  ========  =========================================  ======
Board              Name                   Family    URL                                        Note
=================  =====================  ========  =========================================  ======
msp_exp430f5529lp  MSP430F5529 LaunchPad  msp430    https://www.ti.com/tool/MSP-EXP430F5529LP
msp_exp432e401y    MSP432E401Y LaunchPad  msp432e4  https://www.ti.com/tool/MSP-EXP432E401Y
ek_tm4c123gxl      TM4C123G LaunchPad     tm4c      https://www.ti.com/tool/EK-TM4C123GXL
=================  =====================  ========  =========================================  ======

Tomu
----

=======  ======  ========  =========================  ======
Board    Name    Family    URL                        Note
=======  ======  ========  =========================  ======
fomu     fomu    fomu      https://tomu.im/fomu.html
=======  ======  ========  =========================  ======

WCH
---

================  ================  ========  =====================================================================  ======
Board             Name              Family    URL                                                                    Note
================  ================  ========  =====================================================================  ======
ch32f205r-r0      CH32F205r-r0      ch32f20x  https://github.com/openwch/ch32f20x
ch32v103r_r1_1v0  CH32V103R-R1-1v1  ch32v10x  https://github.com/openwch/ch32v103/tree/main/SCHPCB/CH32V103R-R1-1v1
ch32v203c_r0_1v0  CH32V203C-R0-1v0  ch32v20x  https://github.com/openwch/ch32v20x/tree/main/SCHPCB/CH32V203C-R0
ch32v203g_r0_1v0  CH32V203G-R0-1v0  ch32v20x  https://github.com/openwch/ch32v20x/tree/main/SCHPCB/CH32V203C-R0
nanoch32v203      nanoCH32V203      ch32v20x  https://github.com/wuxx/nanoCH32V203
ch32v307v_r1_1v0  CH32V307V-R1-1v0  ch32v307  https://github.com/openwch/ch32v307/tree/main/SCHPCB/CH32V307V-R1-1v0
================  ================  ========  =====================================================================  ======
