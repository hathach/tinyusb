/* Linker script to configure memory regions. */

SEARCH_DIR(.)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x100000
  EXTFLASH (rx) : ORIGIN = 0x12000000, LENGTH = 0x8000000
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x40000
  CODE_RAM (rwx) : ORIGIN = 0x800000, LENGTH = 0x40000
}


INCLUDE "nrf_common.ld"

/* nrfx v2 linker does not define __tbss_start/end__ __sbss_start/end__*/
/*__tbss_start__ = __tbss_start;*/
/*__tbss_end__ = __tbss_end;*/
/*__sbss_start__ = __sbss_start;*/
/*__sbss_end__ = __sbss_end;*/

/* picolibc crt0 */
/*__data_source = __copy_table_start__;*/
/*__tls_base = __tdata_start;*/
/*PROVIDE( __tls_align = MAX(ALIGNOF(.tdata), ALIGNOF(.tbss)) );*/
/*PROVIDE( __arm32_tls_tcb_offset = MAX(8, __tls_align) );*/
