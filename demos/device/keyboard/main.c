#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <cr_section_macros.h>
#include <NXP/crp.h>
#include "tusb.h"

// Variable to store CRP value in. Will be placed automatically
// by the linker when "Enable Code Read Protect" selected.
// See crp.h header for more information
__CRP const unsigned int CRP_WORD = CRP_NO_CRP ;

int main(void) 
{
  SystemInit();
  tusb_init();

  while (1)
  {

  }

  return 0;
}
