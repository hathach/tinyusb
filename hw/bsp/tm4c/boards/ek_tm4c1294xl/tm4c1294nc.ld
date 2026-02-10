ENTRY(Reset_Handler)

_estack = 0x20008000;    /* end of RAM */
/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0;      /* required amount of heap  */
_Min_Stack_Size = 0x1000; /* required amount of stack */


MEMORY
{
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x00100000
    SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00040000
}

SECTIONS
{
    .text :
    {
        . = ALIGN(4) ;
        _text = . ;
        KEEP(*(.isr_vector))
        *(.text)
        *(.text.*)
        *(.init)
        *(.fini)
        *(.rodata)
        *(.rodata.*)
        *(.ARM.exidx*)
        _etext = . ;
        . = ALIGN(4) ;
    } >FLASH

    .data : AT(ADDR(.text) + SIZEOF(.text))
    {
        _data = .;
        . = ALIGN(4);
        _ldata = LOADADDR (.data);
        *(.data)
        *(.data.*)
        _edata = .;
        . = ALIGN(4);

    } >SRAM

    .bss :
    {
        . = ALIGN(4) ;
        _bss = .;
        *(.bss)
        *(.bss.*)
        *(.COMMON)
        _ebss = .;
        . = ALIGN(4);
    }>SRAM

    /* User_heap_stack section, used to check that there is enough RAM left */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE ( end = . );
        PROVIDE ( _end = . );
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >SRAM
}
