
This includes un-modified version 7.3.0 of the core FreeRTOS files, with the
demos removed from the source tree to save space.

The original files can be downloaded at:
http://www.freertos.org

Information on FreeRTOS licensing is located in the freertos/license.txt file
or go to the website.

 Copyright (C) 1989, 1991 Free Software Foundation, Inc.
                       59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


The following files have been modified to add support for a custom tick. This is
needed for the dual-core M0 FreeRTOS tick function (no sysTick on the LPC4350 M0
core).
software\freertos\freertos\Source\portable\GCC\ARM_CM0\port.c
software\freertos\freertos\Source\portable\IAR\ARM_CM0\port.c
software\freertos\freertos\Source\portable\RVDS\ARM_CM0\port_m0.c
software\freertos\freertos\Source\portable\RVDS\ARM_CM0\portmacro.h