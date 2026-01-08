/* Linker script to configure memory regions. */

SEARCH_DIR(.)
/*GROUP(-lgcc -lc -lnosys) not compatible with clang*/

MEMORY
{
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x100000
  EXTFLASH (rx) : ORIGIN = 0x12000000, LENGTH = 0x8000000
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x40000
  CODE_RAM (rwx) : ORIGIN = 0x800000, LENGTH = 0x40000
}

INCLUDE "nrf_common.ld"
