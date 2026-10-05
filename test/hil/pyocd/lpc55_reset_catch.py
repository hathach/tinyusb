# SPDX-License-Identifier: MIT
# pyocd user script (--script) for LPC55xx. pyocd's reset_and_halt runs a flash-controller
# blank check before the reset, but flash commands need CPU clock <= 100 MHz and prefetch
# off (UM11126 rev 2.1 5.3, Table 113 note 2); under the application's 150 MHz clocks it
# never completes and the next AHB access answers WAIT. A debug-mailbox chip reset first
# resets SYSCON and the flash controller (Table 1058).


def set_reset_catch(core, reset_type):
    target.unlock(target.aps[2])   # DM-AP: chip reset + START_DBG_SESSION (51.6.1)
    return False                   # then pyocd's own reset catch
