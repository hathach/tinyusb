# Build Demos

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**

- [LPCXpresso](#lpcxpresso)
- [Keil](#keil)
- [IAR](#iar)
- [Configure demo](#configure-demo)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

## LPCXpresso

LPCXpresso is an eclipse-based IDE, so you will need to create an workspace first then import project files (.cproject & .project) into it. The following step explain how to do just that. 

1. Click *File->Import*, then expand the *General* folder and select **Existing Projects into Workspace** and click Next.

    ![lpcxpresso_import.png](http://docs.tinyusb.org/images/lpcxpresso_import.png)
    
2. On the next dialog, Click *Browse* and choose the **repo/demos** folder inside the repo. You should see a list of all demo applications. \[**IMPORTANT**\] Make sure the option **Copy projects into workspace** is **CLEAR**, as demo application uses *link folders* and this option may cause problem with the copy import. Then choose any of the demo application and click *Finish*.
    
    ![lpcxpresso_import2.png](http://docs.tinyusb.org/images/lpcxpresso_import2.png)
    
3. Select the configure corresponding to your development board.
    
    ![lpcxpresso_select_board.png](http://docs.tinyusb.org/images/lpcxpresso_select_board.png)
    
4. Then select the correct MCU option from project properties then you are ready to go.
    
    ![lpcxpresso_mcu.png](http://docs.tinyusb.org/images/lpcxpresso_mcu.png)

*TIPS* Working with eclipse-based IDE like lpcxpresso, you should change the indexer option in *Preferences->C/C++->Indexer* to "active build" to have a better code viewer. Those lines that are opt out by #if will be gray, I found this extremely helpful.

![lpcpresso_indexer](http://docs.tinyusb.org/images/lpcxpresso_indexer.png)

## Keil

It is relatively simple for Keil

1. Open the desired demo project e.g *demos/host/host\_freertos/host_freertos.uvproj*
2. Select the configure corresponding to your development board and build it. 
    
    ![keil_select_board.png](http://docs.tinyusb.org/images/keil_select_board.png) 

## IAR

IAR is just as easy as Keil

1. Open the desired demo project e.g *demos/host/host\_freertos/host_freertos.eww*
2. Again select the configure corresponding to your development board and build it. 
    
    ![iar_select_board.png](http://docs.tinyusb.org/images/iar_select_board.png)

## Configure demo

Application demo is written to have the code excluded if its required option is not enabled in [tusb_config.h](). Some of combination may exceed the 32KB limit of IAR/Keil so you may want to re-configure to disable some class support, decrease TUSB_CFG_DEBUG or increase the compiler optimization level.

In addition, there are some configuration you can change such as

- CFG_UART_BAUDRATE in board.h
- CFG_PRINTF_TARGET in the specific board header (e.g board_ea4357.h) to either Semihost, Uart, or SWO.