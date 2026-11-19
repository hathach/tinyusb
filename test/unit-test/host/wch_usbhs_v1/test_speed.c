// SPDX-License-Identifier: MIT
#include "tusb_option.h"
#include "common/tusb_common.h"

TU_VERIFY_STATIC((TUD_OPT_HIGH_SPEED != 0) == EXPECT_DEVICE_HS, "device speed follows its selected controller");
TU_VERIFY_STATIC((TUH_OPT_HIGH_SPEED != 0) == EXPECT_HOST_HS, "host speed follows its selected controller");

int main(void) {
  return 0;
}
