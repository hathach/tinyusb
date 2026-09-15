/*
                  Linker File for S124 MCU
*/

/* Linker script to configure memory regions. */
MEMORY
{
  FLASH (rx)         : ORIGIN = 0x00000000, LENGTH = 0x020000  /* 128K */
  RAM (rwx)          : ORIGIN = 0x20000000, LENGTH = 0x004000  /* 16K  */
  DATA_FLASH (rx)    : ORIGIN = 0x40100000, LENGTH = 0x0001000 /*  4K  */
  ID_CODE_1 (rx)      : ORIGIN = 0x01010018, LENGTH = 0x04     /* 4 bytes */    
  ID_CODE_2 (rx)      : ORIGIN = 0x01010020, LENGTH = 0x04     /* 4 bytes */    
  ID_CODE_3 (rx)      : ORIGIN = 0x01010028, LENGTH = 0x04     /* 4 bytes */    
  ID_CODE_4 (rx)      : ORIGIN = 0x01010030, LENGTH = 0x04     /* 4 bytes */    
}

/* Library configurations */
GROUP(libgcc.a libc.a libm.a libnosys.a)

/* Linker script to place sections and symbol values. Should be used together
 * with other linker script that defines memory regions FLASH and RAM.
 * It references following symbols, which must be defined in code:
 *   Reset_Handler : Entry of reset handler
 *
 * It defines following symbols, which code can use without definition:
 *   __exidx_start
 *   __exidx_end
 *   __copy_table_start__
 *   __copy_table_end__
 *   __zero_table_start__
 *   __zero_table_end__
 *   __etext
 *   __data_start__
 *   __preinit_array_start
 *   __preinit_array_end
 *   __init_array_start
 *   __init_array_end
 *   __fini_array_start
 *   __fini_array_end
 *   __data_end__
 *   __bss_start__
 *   __bss_end__
 *   __end__
 *   end
 *   __HeapLimit
 *   __StackLimit
 *   __StackTop
 *   __stack
 *   __Vectors_End
 *   __Vectors_Size
 */
ENTRY(Reset_Handler)

SECTIONS
{
	.text :
	{
		__ROM_Start = .;

        /* Even though the vector table is not 256 entries (1KB) long, we still allocate that much
         * space because ROM registers are at address 0x400 and there is very little space
         * in between. */
		KEEP(*(.vectors))
		KEEP(*(SORT_BY_NAME(.vector.*)))
		__Vectors_End = .;
		__end__ = .;

		/* ROM Registers start at address 0x00000400 */
		. = __ROM_Start + 0x400;
		KEEP(*(.rom_registers*))

		/* Reserving 0x100 bytes of space for ROM registers. */
		. = __ROM_Start + 0x500;

		/* Vector information array. */
		__Vector_Info_Start = .;
		KEEP(*(SORT_BY_NAME(.vector_info.*)))
		__Vector_Info_End = .;

		/* Hardware lock lookup array. */
		__Lock_Lookup_Start = .;
		KEEP(*(SORT_BY_NAME(.hw_lock_lookup.*)))
		__Lock_Lookup_End = .;

		*(.text*)

		KEEP(*(.init))
		KEEP(*(.fini))

		/* .ctors */
		*crtbegin.o(.ctors)
		*crtbegin?.o(.ctors)
		*(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
		*(SORT(.ctors.*))
		*(.ctors)

		/* .dtors */
 		*crtbegin.o(.dtors)
 		*crtbegin?.o(.dtors)
 		*(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
 		*(SORT(.dtors.*))
 		*(.dtors)

		*(.rodata*)
		__usb_dev_descriptor_start_fs = .;
		KEEP(*(.usb_device_desc_fs*))
		__usb_cfg_descriptor_start_fs = .;
		KEEP(*(.usb_config_desc_fs*))
		__usb_interface_descriptor_start_fs = .;
		KEEP(*(.usb_interface_desc_fs*))
		__usb_descriptor_end_fs = .;

		KEEP(*(.eh_frame*))

		__ROM_End = .;
	} > FLASH = 0xFF

    __Vectors_Size = __Vectors_End - __Vectors;
    __Vector_Info_Size = __Vector_Info_End - __Vector_Info_Start;
    __Lock_Lookup_Size = __Lock_Lookup_End - __Lock_Lookup_Start;

	.ARM.extab :
	{
		*(.ARM.extab* .gnu.linkonce.armextab.*)
	} > FLASH

	__exidx_start = .;
	.ARM.exidx :
	{
		*(.ARM.exidx* .gnu.linkonce.armexidx.*)
	} > FLASH
	__exidx_end = .;

	/* To copy multiple ROM to RAM sections,
	 * uncomment .copy.table section and,
	 * define __STARTUP_COPY_MULTIPLE in startup_ARMCMx.S */
	/*
	.copy.table :
	{
		. = ALIGN(4);
		__copy_table_start__ = .;
		LONG (__etext)
		LONG (__data_start__)
		LONG (__data_end__ - __data_start__)
		LONG (__etext2)
		LONG (__data2_start__)
		LONG (__data2_end__ - __data2_start__)
		__copy_table_end__ = .;
	} > FLASH
	*/

	/* To clear multiple BSS sections,
	 * uncomment .zero.table section and,
	 * define __STARTUP_CLEAR_BSS_MULTIPLE in startup_ARMCMx.S */
	/*
	.zero.table :
	{
		. = ALIGN(4);
		__zero_table_start__ = .;
		LONG (__bss_start__)
		LONG (__bss_end__ - __bss_start__)
		LONG (__bss2_start__)
		LONG (__bss2_end__ - __bss2_start__)
		__zero_table_end__ = .;
	} > FLASH
	*/

	__etext = .;
	
	/* If DTC is used, put the DTC vector table at the start of SRAM.
	   This avoids memory holes due to 1K alignment required by it. */
	.ssp_dtc_vector_table (NOLOAD) : 
	{
		. = ORIGIN(RAM);
		*(.ssp_dtc_vector_table)
	} > RAM

	/* Initialized data section. */
	.data :
	{
		__data_start__ = .;
		*(vtable)
		*(.data*)

		. = ALIGN(4);
		/* preinit data */
		PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP(*(.preinit_array))
		PROVIDE_HIDDEN (__preinit_array_end = .);

		. = ALIGN(4);
		/* init data */
		PROVIDE_HIDDEN (__init_array_start = .);
		KEEP(*(SORT(.init_array.*)))
		KEEP(*(.init_array))
		PROVIDE_HIDDEN (__init_array_end = .);


		. = ALIGN(4);
		/* finit data */
		PROVIDE_HIDDEN (__fini_array_start = .);
		KEEP(*(SORT(.fini_array.*)))
		KEEP(*(.fini_array))
		PROVIDE_HIDDEN (__fini_array_end = .);

		KEEP(*(.jcr*))
		. = ALIGN(4);

		__Code_In_RAM_Start = .;

		KEEP(*(.code_in_ram*))
		__Code_In_RAM_End = .;

		/* Hardware look array. */
		__Lock_Start = .;
		KEEP(*(SORT_BY_NAME(.hw_lock*)))
		__Lock_End = .;

		/* All data end */
		__data_end__ = .;

	} > RAM AT > FLASH

    __Lock_Size = __Lock_End - __Lock_Start;

    .noinit (NOLOAD):
    {
		. = ALIGN(4);
        __noinit_start = .;
        KEEP(*(.noinit*))
        __noinit_end = .;
    } > RAM

	.bss :
	{
		. = ALIGN(4);
		__bss_start__ = .;
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		__bss_end__ = .;
	} > RAM

	.heap (NOLOAD):
	{
        . = ALIGN(8);
		__HeapBase = .;
		__end__ = .;
		end = __end__;
		KEEP(*(.heap*))
		__HeapLimit = .;
	} > RAM

    /* Stacks are stored in this section. */
    .stack_dummy (NOLOAD):
    {
        . = ALIGN(8);
        __StackLimit = .;
        /* Main stack */
        KEEP(*(.stack))
        __StackTop = .;
        /* Thread stacks */
        KEEP(*(.stack*))
        __StackTopAll = .;
    } > RAM

    PROVIDE(__stack = __StackTopAll);

    /* This symbol represents the end of user allocated RAM. The RAM after this symbol can be used
       at run time for things such as ThreadX memory pool allocations. */
    __RAM_segment_used_end__ = ALIGN(__StackTopAll , 4);

	/* Data flash. */
	.dflash :
	{
		__Data_Flash_Start = .;
		KEEP(*(.dflash*))
		__Data_Flash_End = .;
	} > DATA_FLASH

	.id_code_1 :
	{
		__ID_Code_1_Start = .;
		KEEP(*(.id_code_1*))
		__ID_Code_1_End = .;
	} > ID_CODE_1

	.id_code_2 :
	{
		__ID_Code_2_Start = .;
		KEEP(*(.id_code_2*))
		__ID_Code_2_End = .;
	} > ID_CODE_2
	
	.id_code_3 :
	{
		__ID_Code_3_Start = .;
		KEEP(*(.id_code_3*))
		__ID_Code_3_End = .;
	} > ID_CODE_3

	.id_code_4 :
	{
		__ID_Code_4_Start = .;
		KEEP(*(.id_code_4*))
		__ID_Code_4_End = .;
	} > ID_CODE_4	
}
