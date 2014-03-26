# Get Started #

## Download ##

tinyusb uses github as online repository https://github.com/hathach/tinyusb since it is the best place for open source project. 

If you are using Linux, you already know how to what to do. But If Windows is your OS, I would suggest to install [git](http://git-scm.com/) and front-end gui such as [tortoisegit](http://code.google.com/p/tortoisegit) to begin with.

After downloading/cloning, the code base is composed of

Folder  | Description
-----   | -------------
boards  | Source files of supported boards
demos   | Source & project files for demonstration application
docs    | Documentation
mcu     | Low level mcu core & peripheral drivers (e.g CMSIS )
tests   | Unit tests for the stack
tinyusb | All sources files for tinyusb stack itself.
vendor  | Source files from 3rd party such as freeRTOS, fatfs etc ...

## Import & Build ##

*/demos* is the folder where all the application & project files are located. There are demos for both device and hosts. For each, there are different projects for each of supported RTOS. 

### Prerequisites ###

In order to build and run application demo, you would need

- A [supported development board](../../boards/readme.md)
- A supported toolchain: LPCXpresso, Keil, IAR.

### LPCXpresso ###

To prevent any sort of problems, it is recommended to do **EXACTLY** as follows (esp the item 4)

1. Copy the whole repo folder to your lpcxpresso's workspace.
2. Click *File->Import*, then expand the *General* folder and select **Existing Projects into Workspace** and click Next ![lpcxpresso_import.png](lpcxpresso_import.png)
3. On the next dialog, Click *Browse* and choose the **/demos** folder inside the repo. You should see a list of all demo applications ![lpcxpresso_import2.png](lpcxpresso_import2.png).
4. **IMPORTANT** make sure the option **"Copy projects into workspace"** is **clear**. As demo application uses link folders and this option may cause problem for the import.
5. Choose any of the demo application and click *Finish*
6. Select the configure corresponding to your development board ![lpcxpresso_select_config.png](lpcxpresso_select_config.png)
7. Then select the correct MCU option from project properties ![lpcxpresso_mcu.png](lpcxpresso_mcu.png) then you are ready to go.

### Keil ###

It is relatively simple for Keil

1. Open the desired demo project e.g */demos/host/host_freertos/host_freertos.uvproj*
2. Select the configure corresponding to your development board ![keil_select_board.png](keil_select_board.png) and build it. 

### IAR ###

IAR is just as easy as Keil

1. Open the desired demo project e.g */demos/host/host_freertos/host_freertos.eww*
2. Again select the configure corresponding to your development board ![iar_select_board.png](iar_select_board.png) and build it. 

## Configure demo ##

Application demo is written to have the code excluded if its required option is not enabled in [tusb_config.h](). Some of combination may exceed the 32KB limit of IAR/Keil so you may want to re-configure to disable some class support, decrease TUSB_CFG_DEBUG or increase the compiler optimization level.

In addition, there are some configuration you can change such as

- CFG_UART_BAUDRATE in board.h
- CFG_PRINTF_TARGET in the specific board header (e.g board_ea4357.h) to either Semihost, Uart, or SWO.