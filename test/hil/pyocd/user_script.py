# SPDX-License-Identifier: MIT
# pyocd user script, loaded by run_pyocd.py for every HIL session.
from pyocd.target.family.target_lpc5500 import DM_AP, CortexM_LPC5500


def set_reset_catch(core, reset_type):
    # LPC55: pyocd's reset_and_halt runs a flash-controller blank check before the reset, but
    # flash commands need CPU clock <= 100 MHz and prefetch off (UM11126 rev 2.1 5.3, Table 113
    # note 2); under the application's 150 MHz clocks it never completes and the next AHB access
    # answers WAIT. A debug-mailbox chip reset first resets SYSCON and the flash controller
    # (Table 1058; chip reset + START_DBG_SESSION, 51.6.1).
    if isinstance(core, CortexM_LPC5500):
        target.unlock(target.aps[DM_AP])
    return False    # pyocd's own reset catch either way
