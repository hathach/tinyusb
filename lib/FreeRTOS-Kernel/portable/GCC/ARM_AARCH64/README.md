# Armv8-A architecture support

The Armv8-A architecture introduces the ability to use 64-bit and 32-bit
Execution states, known as AArch64 and AArch32 respectively. The AArch64
Execution state supports the A64 instruction set. It holds addresses in 64-bit
registers and allows instructions in the base instruction set to use 64-bit
registers for their processing.

The AArch32 Execution state is a 32-bit Execution state that preserves
backwards compatibility with the Armv7-A architecture, enhancing that profile
so that it can support some features included in the AArch64 state. It supports
the T32 and A32 instruction sets. Follow the
[link](https://developer.arm.com/Architectures/A-Profile%20Architecture)
for more information.

## ARM_AARCH64 port

This port adds support for Armv8-A architecture AArch64 execution state.
This port is generic and can be used as a starting point for Armv8-A
application processors.

* ARM_AARCH64
    * Memory mapped interface to access Arm GIC registers
