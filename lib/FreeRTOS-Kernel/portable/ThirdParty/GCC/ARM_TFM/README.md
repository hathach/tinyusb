# Target of this port

This port adds the support that FreeRTOS applications can call the secure
services in Trusted Firmware M(TF-M) through Platform Security Architecture
(PSA) API based on the ARM Cortex-M23, Cortex-M33, Cortex-M55 and Cortex-M85
platform.

The Platform Security Architecture (PSA) makes it quicker, easier and cheaper
to design security into a device from the ground up. PSA is made up of four key
stages: analyze, architect, implement, and certify. See [PSA Resource Page](https://www.arm.com/architecture/security-features/platform-security).

TF-M is an open source project. It provides a reference implementation of PSA
for Arm M-profile architecture. Please get the details from this [link](https://www.trustedfirmware.org/projects/tf-m/).

# Derivation of the source code

* `os_wrapper_freertos.c`
  The implementation of APIs which are defined in `/interface/include/os_wrapper/mutex.h`
  in trusted-firmware-m (tag: TF-Mv2.0.0). The implementation is based on
  FreeRTOS mutex type semaphore.

# Usage notes

To build a project based on this port:
* Step 1: build the secure image. Please follow the **Build the Secure Side** section for details.
* Step 2: build the nonsecure image. Please follow the **Build the Non-Secure Side** for details.

## Build the Secure Side

### Get the TF-M source code

See the [link](https://git.trustedfirmware.org/TF-M/trusted-firmware-m.git/) to get the source code. This port is supported by TF-M version **tag: TF-Mv2.0.0**.

### Build TF-M

Please refer to this [link](https://trustedfirmware-m.readthedocs.io/en/latest/getting_started/) to build the secure side.
_**Note:** `TFM_NS_MANAGE_NSID` must be configured as "OFF" when building TF-M_.

## Build the Non-Secure Side

Please copy all the files in `freertos_kernel/portable/GCC/ARM_CM[23|33|55|85]_NTZ` into the `freertos_kernel/portable/ThirdParty/GCC/ARM_TFM` folder before using this port. Note that TrustZone is enabled in this port. The TF-M runs in the Secure Side.

Please call the API `tfm_ns_interface_init()` which is defined in `/interface/src/os_wrapper/tfm_ns_interface_rtos.c` by trusted-firmware-m (tag: TF-Mv2.0.0) at the very beginning of your application. Otherwise, it will always fail when calling a TF-M service in the Nonsecure Side.

### Configuration in FreeRTOS kernel

* `configRUN_FREERTOS_SECURE_ONLY`
This macro should be configured as 0. In this port, TF-M runs in the Secure Side while FreeRTOS
Kernel runs in the Non-Secure Side.

* `configENABLE_FPU`
The setting of this macro is decided by the setting in Secure Side which is platform-specific.
If the Secure Side enables Non-Secure access to FPU, then this macro can be configured as 0 or 1. Otherwise, this macro can only be configured as 0.
Please note that Cortex-M23 does not support FPU.
Please refer to [TF-M documentation](https://tf-m-user-guide.trustedfirmware.org/integration_guide/tfm_fpu_support.html) for FPU usage on the Non-Secure side.

* `configENABLE_MVE`
The setting of this macro is decided by the setting in Secure Side which is platform-specific.
If the Secure Side enables Non-Secure access to MVE, then this macro can be configured as 0 or 1. Otherwise, this macro can only be configured as 0.
Please note that only Cortex-M55 and Cortex-M85 support MVE.
Please refer to [TF-M documentation](https://tf-m-user-guide.trustedfirmware.org/integration_guide/tfm_fpu_support.html) for MVE usage on the Non-Secure side.

* `configENABLE_TRUSTZONE`
This macro should be configured as 0 because TF-M doesn't use the secure context management function of FreeRTOS. New secure context management might be introduced when TF-M supports multiple secure context.


### Integrate TF-M Non-Secure interface with FreeRTOS project

To enable calling TF-M services by the Non-Secure Side, the files below should be included in the FreeRTOS project and built together.
* files in `trusted-firmware-m/build/api_ns/interface/src`
  These files contain the implementation of PSA Functional Developer APIs which can be called by Non-Secure Side directly and PSA Firmware Framework APIs in the IPC model. These files should be taken as part of the Non-Secure source code.
* files in `trusted-firmware-m/build/api_ns/interface/include`
  These files are the necessary header files to call TF-M services.
* `trusted-firmware-m/build/api_ns/interface/lib/s_veneers.o`
  This object file contains all the Non-Secure callable functions exported by
  TF-M and it should be linked when generating the Non-Secure image.



*Copyright (c) 2020-2024, Arm Limited. All rights reserved.*
