/* Linker script to configure memory regions. */

SEARCH_DIR(.)
/*GROUP(-lgcc -lc -lnosys) not compatible with clang*/

MEMORY
{
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x80000
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x20000
  CODE_RAM (rwx) : ORIGIN = 0x800000, LENGTH = 0x20000
}


INCLUDE "nrf_common.ld"
