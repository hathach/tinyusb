OUTPUT_FORMAT("elf32-littleriscv")
ENTRY(_start)

__DYNAMIC = 0;

MEMORY {
	csr : ORIGIN = 0x60000000, LENGTH = 0x01000000
	vexriscv_debug : ORIGIN = 0xf00f0000, LENGTH = 0x00000100
	sram : ORIGIN = 0x10000000, LENGTH = 0x00020000
	rom : ORIGIN = 0x2001a000, LENGTH = 0x00200000 - 0x1a000
}

SECTIONS
{
	.text :
	{
		_ftext = .;
		*(.text.start)
		*(.text .stub .text.* .gnu.linkonce.t.*)
		_etext = .;
	} > rom

	.rodata :
	{
		. = ALIGN(4);
		_frodata = .;
		*(.rodata .rodata.* .gnu.linkonce.r.*)
		*(.rodata1)
		*(.srodata)
		_erodata = .;
	} > rom

	.data : AT (ADDR(.rodata) + SIZEOF (.rodata))
	{
		. = ALIGN(4);
		_fdata = .;
		*(.data .data.* .gnu.linkonce.d.*)
		*(.data1)
		_gp = ALIGN(16);
		*(.sdata .sdata.* .gnu.linkonce.s.* .sdata2 .sdata2.*)
		_edata = ALIGN(16); /* Make sure _edata is >= _gp. */
	} > sram

	.bss :
	{
		. = ALIGN(4);
		_fbss = .;
		*(.dynsbss)
		*(.sbss .sbss.* .gnu.linkonce.sb.*)
		*(.scommon)
		*(.dynbss)
		*(.bss .bss.* .gnu.linkonce.b.*)
		*(COMMON)
		. = ALIGN(4);
		_ebss = .;
		_end = .;
        PROVIDE(end = .);
	} > sram
}

PROVIDE(_fstack = ORIGIN(sram) + LENGTH(sram) - 4);
