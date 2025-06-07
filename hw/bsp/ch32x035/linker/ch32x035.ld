/*
 * Placeholder linker script for CH32X035 (likely ARM Cortex-M0/M0+)
 * Please verify and update FLASH and RAM origins and lengths.
 * #error "This is a placeholder linker script. Please verify memory regions and sections!"
 */

ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 62K /* TODO: Adjust X035 actual flash size (e.g. 62K for CH32X035F8P6) */
  RAM (xrw)  : ORIGIN = 0x20000000, LENGTH = 10K /* TODO: Adjust X035 actual RAM size (e.g. 10K for CH32X035F8P6) */
}

_estack = ORIGIN(RAM) + LENGTH(RAM); /* End of RAM */

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb */
    *(.glue_7t)        /* glue thumb to arm */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >FLASH

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >RAM AT> FLASH

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + MIN(_Min_Heap_Size, 0); /* Reserve space for heap if Minimal Heap Size is defined */
    . = . + _Min_Stack_Size; /* Reserve space for stack */
    . = ALIGN(8);
  } >RAM
}

/* Define default heap and stack sizes if not provided by user */
/* These are minimal values for a basic TinyUSB example. Adjust as needed. */
_Min_Heap_Size = 0x100 ; /* Minimal Heap Size e.g. 256 bytes */
_Min_Stack_Size = 0x200 ; /* Minimal Stack Size e.g. 512 bytes */
