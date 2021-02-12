/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x1000, LENGTH = 0xE0000-0x1000
  RAM (rwx) :  ORIGIN = 0x20000008, LENGTH = 0x3fff8
}


INCLUDE "nrf_common.ld"
