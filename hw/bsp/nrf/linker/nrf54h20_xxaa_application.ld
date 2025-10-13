/* Linker script to configure memory regions. */

SEARCH_DIR(.)
/*GROUP(-lgcc -lc) not compatible with clang*/

MEMORY
{
  FLASH (rx) : ORIGIN = 0xE0A0000, LENGTH = 0x40000 /* Inside global MRAM0 */
  FLASH1 (rx) : ORIGIN = 0x2F840000, LENGTH = 0x4000 /* OTP0 */
  EXTFLASH (rx) : ORIGIN = 0x70000000, LENGTH = 0x20000000
  RAM (rwx) : ORIGIN = 0x22000000, LENGTH = 0x8000
  RAM1 (rwx) : ORIGIN = 0x2F000000, LENGTH = 0x80000 /* RAM00 */
  RAM2 (rwx) : ORIGIN = 0x2F080000, LENGTH = 0x60000 /* RAM01 */
  RAM3 (rwx) : ORIGIN = 0x2F880000, LENGTH = 0x10000 /* RAM20 */
  RAM4 (rwx) : ORIGIN = 0x2F890000, LENGTH = 0x8000 /* RAM21 */
  RAM5 (rwx) : ORIGIN = 0x2FC00000, LENGTH = 0x4000 /* RAM30 (low-speed) */
  RAM6 (rwx) : ORIGIN = 0x2FC04000, LENGTH = 0x4000 /* RAM31 (low-speed) */
}


INCLUDE "nrf_common.ld"
