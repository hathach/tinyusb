/* ENTRY(Reset_Handler) */ 

MEMORY 
{
    FLASH(rx) : ORIGIN = 0x00000000, LENGTH = 256K
    SRAM(rwx) : ORIGIN = 0x20000000, LENGTH = 32K
}

SECTIONS 
{
    .text : 
    {
        . = ALIGN(4) ;
        *(.vectors) 
        *(.text) 
        *(.text.*)
        *(.init)
        *(.fini)
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4) ; 
        __end_text = . ; 
    } >FLASH
    
    .data : AT(ADDR(.text) + SIZEOF(.text))
    {
        . = ALIGN(4);
        __start_data = . ; 
        __la_data = LOADADDR(.data);
        *(.data) 
        *(.data.*)
        . = ALIGN(4);
        __end_data = . ; 

    } >SRAM

    .bss :
    {
        . = ALIGN(4) ;
        __start_bss = . ;
        __bss_start__ = __start_bss;
        *(.bss)
        *(.bss.*)
        *(.COMMON) 
        __end_bss = . ;
        . = ALIGN(4);
    }>SRAM

}
