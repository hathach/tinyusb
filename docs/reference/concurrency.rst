***********
Concurrency
***********

The TinyUSB library is designed to operate on single-core MCUs with multi-threaded applications in mind. Interaction with interrupts is especially important to pay attention to.
It is compatible with optionally using a RTOS.

General
-------

When writing code, keep in mind that the OS (if using a RTOS) may swap out your code at any time. Also, your code can be preempted by an interrupt at any time.

Application Code
----------------

The USB core does not execute application callbacks while in an interrupt context. Calls to application code are from within the USB core task context. Note that the application core will call class drivers from within their own task.

Class Drivers
-------------

Class driver code should never be called from an interrupt context by the USB core, though the application is allowed to call class driver functions from interrupts. USB core functions may be called simultaneously by multiple tasks. Use care that proper locking is used to guard the USBD core functions from this case.

Class drivers are allowed to call ``usbd_*`` functions, but not ``dcd_*`` functions.

USB Core
--------

All functions that may be called from an (USB core) interrupt context have a ``bool in_isr`` parameter to remind the implementer that special care must be taken.

Interrupt handlers must not directly call class driver code, they must pass a message to the USB core's task.

 ``usbd_*`` functions may be called from interrupts without any notice. They may also be called simultaneously by multiple tasks.

Device Drivers
--------------

Much of the processing of the USB stack is done in an interrupt context, and care must be taken in order to ensure variables are handled in the appropriate ways by the compiler and optimizer.

In particular:

*  Ensure that all memory-mapped registers (including packet memory) are marked as volatile. GCC's optimizer will even combine memory access (like two 16-bit to be a 32-bit) if you don't mark the pointers as volatile. On some architectures, this can use macros like _I , _O , or _IO.
*  All defined global variables are marked as  ``static``.
