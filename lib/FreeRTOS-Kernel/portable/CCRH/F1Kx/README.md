# RH850/F1K and F1Kx FreeRTOS Port with CC-RH Compiler

## Introduction

This repository contains the port of FreeRTOS for Renesas RH850/F1K and F1Kx microcontrollers using the CC-RH compiler. The following sections provide instructions on how to use this port, a link to the test project, and other relevant information.

## Prerequisites
- Compiler: CC-RH
- FreeRTOS version 11.1.0

| Device   | FPU | SMP |
|----------|-----|-----|
| F1K      | Yes | No  |
| F1KM-S1  | Yes | No  |
| F1KM-S2  | Yes | No  |
| F1KM-S4  | Yes | No  |
| F1KH-D8  | Yes | Yes |

## Link to Test Project

The test project can be found [here](https://github.com/FreeRTOS/FreeRTOS-Community-Supported-Demos) (`RH850_F1Kx_CCRH`). This project contains example tasks and configurations to help you get started with FreeRTOS on the RH850/F1K and F1Kx.

## Note
   1. Configure IPIR Interrupt: Ensure that the bit specifying the destination for binding (requesting) an interrupt is enabled (e.g: IBDxxx register of F1KH-D8) (1)
   2. `Channel 0` and address `0xFFFEEC00` are used as default configuration for configIPIR_CHANNEL and configEXCLUSIVE_ADDRESS, in case of resource confliction other channel/address can be used. (2)
   3. The minimal stack size (configMINIMAL_STACK_SIZE) must be included the reserved memory for nested interrupt. This formula can be referred: `(task_context_size) * (2 + configMAX_INT_NESTING) + Stack_depth_of_taskcode`
   In which, `task_context_size` is calculated as `36*4bytes = 144bytes` (when FPU enabled) or `34*4bytes = 136` (when FPU disabled), configMAX_INT_NESTING is `02` as default (Note that a value of `0` is not allowed).
   4. `configTIMER_PRESCALE`: This value is required in order to correctly configure clock for `CPUCLK_L`. Refer to Hardware Manual at `Table 44.22` for `option byte`: If the user sets the option byte `CKDIVMD to 1`, then `configTIMER_PRESCALE = 4`. Otherwise, if `CKDIVMD is set to 0`, then `configTIMER_PRESCALE = 2`.

(1)  This is applicable for F1KH-D8 with SMP only.

(2) This is optional and applicable for SMP only.

## Other Relevant Information

- **Documentation:**
  - Refer to the official [FreeRTOS documentation](https://www.freertos.org/Documentation/RTOS_book.html) for detailed information on configuring and using FreeRTOS.
  - Consult the [RH850 F1K group user manual hardware manual](https://www.renesas.com/us/en/document/mah/rh850f1k-group-users-manual-hardware?r=1170166) for specific details about the microcontroller.
  - For more information about Renesas RH850/F1K and F1Kx, please visit [this website](https://www.renesas.com/us/en/products/microcontrollers-microprocessors/rh850-automotive-mcus)
  - The CC-RH compiler can be downloaded [here](https://www.renesas.com/us/en/software-tool/c-compiler-package-rh850-family#downloads)

- **Support:**
  - If you encounter any issues or have questions about this port, please open an issue in this repository or contact the maintainer.

- **Contributing:**
  - Contributions to improve this port are welcome. Please fork the repository, make your changes, and submit a pull request.