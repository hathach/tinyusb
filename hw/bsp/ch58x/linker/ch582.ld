/* CH582 Linker Script for TinyUSB
 * Based on WCH CH583 SDK Link.ld
 * Supports parameterized flash/ram sizes via --defsym
 */

/* Default sizes if not provided via --defsym */
__flash_size = DEFINED(__FLASH_SIZE) ? __FLASH_SIZE : 448K;
__ram_size = DEFINED(__RAM_SIZE) ? __RAM_SIZE : 32K;
__stack_size = DEFINED(__STACK_SIZE) ? __STACK_SIZE : 2048;

ENTRY( _start )

PROVIDE( _stack_size = __stack_size );

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = __flash_size
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = __ram_size
}

SECTIONS
{
  .init :
  {
    _sinit = .;
    . = ALIGN(4);
    KEEP(*(SORT_NONE(.init)))
    . = ALIGN(4);
    _einit = .;
  } >FLASH AT>FLASH

  .highcodelalign :
  {
    . = ALIGN(4);
    PROVIDE(_highcode_lma = .);
  } >FLASH AT>FLASH

  .highcode :
  {
    . = ALIGN(4);
    PROVIDE(_highcode_vma_start = .);
    *(.vector);
    KEEP(*(SORT_NONE(.vector_handler)))
    *(.highcode);
    *(.highcode.*);
    . = ALIGN(4);
    PROVIDE(_highcode_vma_end = .);
  } >RAM AT>FLASH

  .text :
  {
    . = ALIGN(4);
    KEEP(*(SORT_NONE(.handle_reset)))
    *(.text)
    *(.text.*)
    *(.rodata)
    *(.rodata*)
    *(.sdata2.*)
    *(.glue_7)
    *(.glue_7t)
    *(.gnu.linkonce.t.*)
    . = ALIGN(4);
  } >FLASH AT>FLASH

  .fini :
  {
    KEEP(*(SORT_NONE(.fini)))
    . = ALIGN(4);
  } >FLASH AT>FLASH

  PROVIDE( _etext = . );
  PROVIDE( _eitcm = . );

  .preinit_array :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH AT>FLASH

  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
    KEEP (*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .ctors))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH AT>FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
    KEEP (*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .dtors))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH AT>FLASH

  .ctors :
  {
    KEEP (*crtbegin.o(.ctors))
    KEEP (*crtbegin?.o(.ctors))
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
  } >FLASH AT>FLASH

  .dtors :
  {
    KEEP (*crtbegin.o(.dtors))
    KEEP (*crtbegin?.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
  } >FLASH AT>FLASH

  .dlalign :
  {
    . = ALIGN(4);
    PROVIDE(_data_lma = .);
  } >FLASH AT>FLASH

  .data :
  {
    . = ALIGN(4);
    PROVIDE(_data_vma = .);
    *(.gnu.linkonce.r.*)
    *(.data .data.*)
    *(.gnu.linkonce.d.*)
    . = ALIGN(8);
    PROVIDE( __global_pointer$ = . + 0x800 );
    *(.sdata .sdata.*)
    *(.gnu.linkonce.s.*)
    . = ALIGN(8);
    *(.srodata.cst16)
    *(.srodata.cst8)
    *(.srodata.cst4)
    *(.srodata.cst2)
    *(.srodata .srodata.*)
    . = ALIGN(4);
    PROVIDE( _edata = .);
  } >RAM AT>FLASH

  .bss :
  {
    . = ALIGN(4);
    PROVIDE( _sbss = .);
    *(.sbss*)
    *(.gnu.linkonce.sb.*)
    *(.bss*)
    *(.gnu.linkonce.b.*)
    *(COMMON*)
    . = ALIGN(4);
    PROVIDE( _ebss = .);
  } >RAM AT>FLASH

  PROVIDE( _end = _ebss);
  PROVIDE( end = . );

  .stack ORIGIN(RAM) + LENGTH(RAM) - __stack_size :
  {
    PROVIDE( _heap_end = . );
    . = ALIGN(4);
    PROVIDE(_susrstack = . );
    . = . + __stack_size;
    PROVIDE( _eusrstack = .);
    __freertos_irq_stack_top = .;
  } >RAM
}
