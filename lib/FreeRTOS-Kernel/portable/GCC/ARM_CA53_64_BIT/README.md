# ARM_CA53_64_BIT port

Initial port to support Armv8-A architecture in FreeRTOS kernel was written for
Arm Cortex-A53 processor.

* ARM_CA53_64_BIT
    * Memory mapped interface to access Arm GIC registers

This port is generic and can be used as a starting point for other Armv8-A
application processors. Therefore, the port `ARM_CA53_64_BIT` is renamed as
`ARM_AARCH64`. The existing projects that use old port `ARM_CA53_64_BIT`,
should migrate to renamed port `ARM_AARCH64`.

**NOTE**

This port uses memory mapped interface to access Arm GIC registers.
