/* Linker script to configure memory regions. */

SEARCH_DIR(.)
/*GROUP(-lgcc -lc) not compatible with clang*/

MEMORY
{
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x1FD000 /* Inside global RRAM0 */
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x40000
  RAM1 (rwx) : ORIGIN = 0x20040000, LENGTH = 0x40000
}

INCLUDE "nrf_common.ld"
