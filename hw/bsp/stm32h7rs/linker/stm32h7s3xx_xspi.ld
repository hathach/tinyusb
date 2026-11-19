/*
 * STM32H7S3xx: boot from internal flash, run from AXI SRAM, image stored on XSPI2 flash
 *
 * - FLASH (internal, 64 KB): vector table and the boot code (.boot_text, xspi_boot.c)
 * - XSPI  (0x70000000):      load image of everything else (code, constants, .data)
 * - RAM   (AXI SRAM):        the image runs here, copied by Reset_Handler, then .bss
 * - DTCM:                    stack and heap
 *
 * AXI SRAM is 456 KB with the default option bytes (no ITCM/DTCM share, no ECC).
 */

ENTRY(Reset_Handler)

_Min_Heap_Size = 0x200;  /* required amount of heap */
_Min_Stack_Size = 0x400; /* required amount of stack */

/* MPU_Config() makes this region executable: keep it the internal flash, the RAM image
   runs from the default memory map so the non-cacheable region below keeps its attributes */
__FLASH_BEGIN  = 0x08000000;
__FLASH_SIZE   = 0x00010000;

__XSPI_BEGIN   = 0x70000000;
__XSPI_SIZE    = 0x02000000;

__RAM_BEGIN    = 0x24000000;
__RAM_SIZE     = 0x71000;
__RAM_NONCACHEABLEBUFFER_SIZE = 0x1000; /* RTT control block and buffers (BUFFER_SIZE_UP) */

MEMORY
{
  FLASH      (rx) : ORIGIN = __FLASH_BEGIN, LENGTH = __FLASH_SIZE
  XSPI       (r)  : ORIGIN = __XSPI_BEGIN,  LENGTH = __XSPI_SIZE
  RAM       (xrw) : ORIGIN = __RAM_BEGIN,   LENGTH = __RAM_SIZE
  RAM_NONCACHEABLEBUFFER (rw) : ORIGIN = __RAM_BEGIN + __RAM_SIZE, LENGTH = __RAM_NONCACHEABLEBUFFER_SIZE

  ITCM      (xrw) : ORIGIN = 0x00000000, LENGTH = 0x00010000
  DTCM       (rw) : ORIGIN = 0x20000000, LENGTH = 0x00010000
  SRAMAHB    (rw) : ORIGIN = 0x30000000, LENGTH = 0x00008000
  BKPSRAM    (rw) : ORIGIN = 0x38800000, LENGTH = 0x00001000
}

/* Highest address of the user mode stack */
_estack = ORIGIN(DTCM) + LENGTH(DTCM);

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* Runs before the image is in RAM */
  .boot :
  {
    . = ALIGN(4);
    *(.boot_text)
    *(.boot_text*)
    . = ALIGN(4);
  } >FLASH

  /* RAM image: each section's load address keeps its offset from the image start, so
     Reset_Handler copies [__ram_image_start, __ram_image_end) from __ram_image_lma */
  .text :
  {
    . = ALIGN(8);
    __ram_image_start = .;
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;
  } >RAM AT>XSPI

  __ram_image_lma = LOADADDR(.text);

  .rodata : AT(__ram_image_lma + ADDR(.rodata) - __ram_image_start)
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >RAM

  .ARM.extab : AT(__ram_image_lma + ADDR(.ARM.extab) - __ram_image_start)
  {
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
  } >RAM

  .ARM : AT(__ram_image_lma + ADDR(.ARM) - __ram_image_start)
  {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } >RAM

  .preinit_array : AT(__ram_image_lma + ADDR(.preinit_array) - __ram_image_start)
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } >RAM

  .init_array : AT(__ram_image_lma + ADDR(.init_array) - __ram_image_start)
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(4);
  } >RAM

  .fini_array : AT(__ram_image_lma + ADDR(.fini_array) - __ram_image_start)
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >RAM

  .data : AT(__ram_image_lma + ADDR(.data) - __ram_image_start)
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    *(.RamFunc)
    *(.RamFunc*)
    . = ALIGN(4);
    _edata = .;
    __ram_image_end = .;
  } >RAM

  /* referenced by the (unused, weak) startup Reset_Handler */
  _sidata = LOADADDR(.data);

  . = ALIGN(4);
  .bss (NOLOAD) :
  {
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  RW_NONCACHEABLE :
  {
    __NONCACHEABLEBUFFER_BEGIN = .;
    KEEP(*(noncacheable_buffer))
    __NONCACHEABLEBUFFER_END = .;
  } > RAM_NONCACHEABLEBUFFER

  /* User_heap_stack section, used to check that there is enough "DTCM" Ram type memory left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >DTCM

  /* Remove information from the compiler libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }

  ASSERT(LOADADDR(.data) + SIZEOF(.data) - __ram_image_lma == __ram_image_end - __ram_image_start,
         "RAM image load layout is not contiguous")
}
