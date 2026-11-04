/* Linker script for WCH CH565/CH569, based on hydrausb3/wch-ch56x-bsp ld/Link.ld.
 * FLASH: 448K code flash @0x00000000
 * RAM (RAMS): 16K general SRAM @0x20000000
 * RAMX: 32K (default chip config) high-speed SRAM @0x20020000 - the only RAM reachable by
 *       high-speed peripheral DMA (USB2.0/USB3.0/ETH/HSPI/SerDes). USB buffers go to the
 *       16-byte aligned NOLOAD .dmadata section below, which the SDK startup
 *       (startup_CH56x.S) clears before main.
 */
ENTRY( _start )

__flash_size = DEFINED(__FLASH_SIZE) ? __FLASH_SIZE : 448K;
__ram_size   = DEFINED(__RAM_SIZE)   ? __RAM_SIZE   : 16K;
__ramx_size  = DEFINED(__RAMX_SIZE)  ? __RAMX_SIZE  : 32K;
__stack_size = DEFINED(__STACK_SIZE) ? __STACK_SIZE : 2048;

PROVIDE( _stack_size = __stack_size );

MEMORY
{
	FLASH (rx) : ORIGIN = 0x00000000, LENGTH = __flash_size
	RAM (xrw) : ORIGIN = 0x20000000, LENGTH = __ram_size
	RAMX (xrw) : ORIGIN = 0x20020000, LENGTH = __ramx_size
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

	.vector :
	{
		*(.vector);
		. = ALIGN(64);
	} >FLASH AT>FLASH

	.text :
	{
		. = ALIGN(4);
		*(.text)
		*(.text.*)
		*(.rodata)
		*(.rodata*)
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

	.preinit_array  :
	{
	  PROVIDE_HIDDEN (__preinit_array_start = .);
	  KEEP (*(.preinit_array))
	  PROVIDE_HIDDEN (__preinit_array_end = .);
	} >FLASH AT>FLASH

	.init_array     :
	{
	  PROVIDE_HIDDEN (__init_array_start = .);
	  KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
	  KEEP (*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .ctors))
	  PROVIDE_HIDDEN (__init_array_end = .);
	} >FLASH AT>FLASH

	.fini_array     :
	{
	  PROVIDE_HIDDEN (__fini_array_start = .);
	  KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
	  KEEP (*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .dtors))
	  PROVIDE_HIDDEN (__fini_array_end = .);
	} >FLASH AT>FLASH

	.ctors          :
	{
	  KEEP (*crtbegin.o(.ctors))
	  KEEP (*crtbegin?.o(.ctors))
	  KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .ctors))
	  KEEP (*(SORT(.ctors.*)))
	  KEEP (*(.ctors))
	} >FLASH AT>FLASH

	.dtors          :
	{
	  KEEP (*crtbegin.o(.dtors))
	  KEEP (*crtbegin?.o(.dtors))
	  KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .dtors))
	  KEEP (*(SORT(.dtors.*)))
	  KEEP (*(.dtors))
	} >FLASH AT>FLASH

	.dalign :
	{
		. = ALIGN(4);
		PROVIDE(_data_vma = .);
	} >RAM AT>FLASH

	.dlalign :
	{
		. = ALIGN(4);
		PROVIDE(_data_lma = .);
	} >FLASH AT>FLASH

	.data :
	{
		*(.gnu.linkonce.r.*)
		*(.data .data.*)
		*(.gnu.linkonce.d.*)
		. = ALIGN(8);
		PROVIDE( __global_pointer$ = . + 0x800 );
		*(.sdata .sdata.*)
		*(.sdata2.*)
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

	/* UNINITIALISED ONLY. This is a NOLOAD (bss-like) region: startup_CH56x.S zeroes it between
	   _dmadata_start and _dmadata_end, and nothing copies an LMA into it. An object placed here
	   WITH an initialiser silently loses it - the bytes are dropped from the .bin and the startup
	   zero-fill runs over them, with no diagnostic from the compiler, linker or objcopy.
	   That matters because CFG_TUSB_MEM_SECTION is defined to .dmadata for this family
	   (hw/bsp/ch569/family.cmake, family.mk), so any driver or application object can land here.
	   Keep DMA buffers uninitialised and assign at runtime. No AT> region: a load address is
	   meaningless for a section that is never loaded. */
	.DMADATA (NOLOAD) :
	{
		. = ALIGN(16);
		PROVIDE( _dmadata_start = .);
		*(.dmadata*)
		*(.dmadata.*)
		*(.DMADATA*)
		. = ALIGN(16);
		PROVIDE( _dmadata_end = .);
	} >RAMX

	.stack ORIGIN(RAM) + LENGTH(RAM) - __stack_size :
	{
		. = ALIGN(4);
		PROVIDE(_susrstack = . );
		. = . + __stack_size;
		PROVIDE( _eusrstack = .);
	} >RAM
}
