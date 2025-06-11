# Analog Devices MAXIM

This BSP is for working with the Analog microcontrollers
 - [MAX32650](https://www.analog.com/en/products/max32650.html),
 - [MAX32651](https://www.analog.com/en/products/max32651.html)
 - [MAX32652](https://www.analog.com/en/products/max32652.html)
 - [MAX32665](https://www.analog.com/en/products/max32665.html)
 - [MAX32666](https://www.analog.com/en/products/max32666.html)
 - [MAX32690](https://www.analog.com/en/products/max32690.html)
 - [MAX78002](https://www.analog.com/en/products/max78002.html) AI microcontroller.

The following boards are supported:
 * [MAX32650EVKIT](https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/max32650-evkit.html)
 * [MAX32650FTHR](https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/max32650fthr.html)
 * [MAX32651EVKIT](https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/max32651-evkit.html) (Secure Bootloader)
 * [MAX32666EVKIT](https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/max32666evkit.html)
 * [MAX32666FTHR](https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/max32666fthr.html)
 * [MAX32690EVKIT](https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/max32690evkit.html)
 * [AD-APARD32690-SL](https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/ad-apard32690-sl.html)
 * [MAX78002EVKIT](https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/max78002evkit.html)

This part family leverages the Maxim Microcontrollers SDK (MSDK) for the device
interfaces and hardware abstraction layers. This source code package is fetched
as part of the get-deps script.

The microcontroller utilizes the standard GNU ARM toolchain.  If this toolchain
is not already available on your build machine, it can be installed by using the
bundled MSDK installation.  Details on downloading and installing can be found
in the [User's Guide](https://analogdevicesinc.github.io/msdk//USERGUIDE/).

## Flashing

The default flashing behavior in this BSP is to utilize JLink.  This can be done
by running the `flash` or `flash-jlink` rule for Makefiles, or the
`<target>-jlink` target for CMake.

Most the Evaluation Kit and boards are shipped with a CMSIS-DAP
compatible debug probe. However, at the time of writing, the necessary flashing
algorithms for OpenOCD have not yet been incorporated into the OpenOCD master
branch.  To utilize the provided debug probes, please install the bundled MSDK
package which includes the appropriate OpenOCD modifications.   To leverage this
OpenOCD instance, run the `flash-msdk` Makefile rule, or `<target>-openocd` CMake
target.
