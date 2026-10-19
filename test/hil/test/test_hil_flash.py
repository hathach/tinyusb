#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# The rosters checked by the rules of agentrc's flash provider (lib/agentrc at the pin,
# hil_util.provider()), which test_change_impact.py cannot call: build.yml runs it without
# lib/agentrc. The provider's own rules are agentrc's tests (tests/test_hil_flash.py). Run directly:
#   python3 test/hil/test/test_hil_flash.py
import json
import os
import re
import sys
import unittest
from pathlib import Path

HIL_DIR = Path(__file__).resolve().parents[1]
REPO = str(HIL_DIR.parents[1])
sys.path.insert(0, str(HIL_DIR))
sys.path.insert(0, os.path.join(REPO, 'tools'))
import change_impact  # noqa: E402
import hil_flash  # noqa: E402
from test_change_impact import roster_flashers  # noqa: E402


class Rosters(unittest.TestCase):
    """The committed rosters, checked by the provider's rules."""

    def test_roster_pyocd_entries_filter_vid_pid(self):
        # unfiltered, pyocd opens every CMSIS-DAP-class device on the rig (the provider's pyocd launcher)
        for path, board in roster_flashers():
            self.assertFalse(hil_flash.unfiltered_pyocd(board), f"{path}: {board['name']}")

    def test_roster_recover_entries_are_demonstrated_openocd_ones(self):
        """Every `flasher_recover` in a HIL config is openocd over the SAME probe as its jlink,
        stlink or lm4flash primary and dispatches. The stlink ones (stm32h743nucleo,
        stm32g0b1nucleo, stm32u083nucleo) reset 3/3 and reflashed on ci.lan 2026-10-06.
        ek_tm4c123gxl's too (2026-10-06), only with its reset-assert-post sleep: without it hla's
        DEMCR write ~3 ms after SYSRESETREQ fails (0x7) and openocd returns rc 1 though the DUT
        reset. The 100 ms is empirical; the datasheet's software reset is ~2 us nominal and the
        debug registers stay reachable in reset, so the ICDI/hla path is the likely party
        (unverified). The
        jlink set is the boards whose reset over
        openocd+jlink was demonstrated on their roster's rig, and flash too wherever openocd can
        flash the part (ci.lan 2026-08-17, 2026-09-21, 2026-09-22 and 2026-10-02, tusb 2026-09-21 for
        lpcxpresso43s67). A new one is added only
        after the same demonstration. frdm_k64f is host-only, so usbtest never marks
        it; its entry serves a manual reset through the recovery flasher. Left out, and why: stm32f769disco (probe not on the rig to demonstrate).
        ra4m1_ek is reset-only (openocd 0ce743125 has no RA target cfg or flash driver): 20/20
        resets re-enumerated it and its wedge drill passed (2026-10-06); the ~80% seen 2026-09-22
        did not reproduce in 70 resets. ra6m5_ek is reset-only the same way (SYSRESETREQ is an
        internal MCU reset, RA6M5 UM 5.3.7): 20/20 resets re-enumerated it at 480 Mbps and 3/3
        wedge drills passed (ci.lan 2026-10-09).
        mimxrt1064_evk is reset-only: openocd 0ce743125 has no target cfg and no flash driver for
        it (FlexSPI), so its entry declares a bare SWD DAP and Cortex-M target with SYSRESETREQ;
        3/3 resets each re-enumerated the DUT (2026-09-22). Same trade as
        nrf54lm20dk below. nrf54lm20dk's reset works (libjaylink 0.5.0 finds its PID 0x1069; 0.4.0 did
        not) but its reflash cannot: nrf54l.cfg declares no RRAM flash bank. Accepted trade:
        the entry arms in-run recovery (a reset) where JLinkExe allowed none; a wedge the reset
        does not clear stays wedged for the run. lpcxpresso55s69 is reset-only like mimxrt1064_evk
        (no LPC55 target cfg or flash driver in 0ce743125), its DAP pinned to DPIDR 0x6ba02477 and
        AP 0; SYSRESETREQ is a chip reset (UM11126 4.6.1) and libjaylink finds its LPCXpresso V2
        probe (PID 0x1024) from 0.4.0; 3/3 resets each re-enumerated the DUT (ci.lan 2026-10-17).
        The STM32 entries empty
        their target's reset-init event: stm32f0x/f4x/f7x/l4x.cfg set `adapter speed 8000` there,
        these on-board J-Link probes cap at 4000 and fail even at 4000 (auto_probe failed, SWD
        parity mismatch; stm32f407disco and stm32f072disco, 2026-09-22), and `program` runs
        reset-init after anything the args set -- so no `adapter speed` can fix it, and one
        before the cfg is overridden anyway. `adapter speed` goes AFTER a target cfg that pins its
        own: lpc1xxx.cfg sets 10 kHz, which made lpcxpresso11u37's flash take 118 s. The LPC
        entries (lpc11xx, lpc40xx, lpc4357) keep verify off: openocd rewrites the vector checksum word
        the ELF lacks, so read-back never equals the image."""
        demonstrated = {'nrf52840dk', 'metro_m4_express', 'stm32f072disco',
                        'stm32f407disco', 'stm32f723disco', 'stm32l476disco', 'lpcxpresso11u37',
                        'ea4088_quickstart', 'nrf54lm20dk', 'nrf5340dk', 'frdm_k64f', 'mimxrt1064_evk',
                        'lpcxpresso43s67',   # hfp.json, demonstrated on tusb
                        'stm32h743nucleo', 'stm32g0b1nucleo', 'stm32u083nucleo', 'ek_tm4c123gxl',
                        'ra4m1_ek', 'ra6m5_ek', 'lpcxpresso55s69'}
        reset_only = {'mimxrt1064_evk': ('-f interface/jlink.cfg -c "transport select swd" -c "adapter speed 1000" '
                                         '-c "swd newdap rt1064 cpu -expected-id 0" '
                                         '-c "dap create rt1064.dap -chain-position rt1064.cpu" '
                                         '-c "target create rt1064.cpu cortex_m -dap rt1064.dap" '
                                         '-c "cortex_m reset_config sysresetreq"'),
                      'ra4m1_ek': ('-f interface/jlink.cfg -c "transport select swd" -c "adapter speed 1000" '
                                   '-c "swd newdap ra4m1 cpu -expected-id 0" '
                                   '-c "dap create ra4m1.dap -chain-position ra4m1.cpu" '
                                   '-c "target create ra4m1.cpu cortex_m -dap ra4m1.dap" '
                                   '-c "cortex_m reset_config sysresetreq"'),
                      'ra6m5_ek': ('-f interface/jlink.cfg -c "transport select swd" -c "adapter speed 1000" '
                                   '-c "swd newdap ra6m5 cpu -expected-id 0" '
                                   '-c "dap create ra6m5.dap -chain-position ra6m5.cpu" '
                                   '-c "target create ra6m5.cpu cortex_m -dap ra6m5.dap" '
                                   '-c "cortex_m reset_config sysresetreq"'),
                      'lpcxpresso55s69': ('-f interface/jlink.cfg -c "transport select swd" -c "adapter speed 1000" '
                                          '-c "swd newdap lpc55s69 cpu -expected-id 0x6ba02477" '
                                          '-c "dap create lpc55s69.dap -chain-position lpc55s69.cpu" '
                                          '-c "target create lpc55s69.cpu cortex_m -dap lpc55s69.dap -ap-num 0" '
                                          '-c "cortex_m reset_config sysresetreq"')}
        stlink_targets = {'stm32h743nucleo': 'stm32h7x', 'stm32g0b1nucleo': 'stm32g0x',
                          'stm32u083nucleo': 'stm32u0x'}
        tm4c_args = ('-f interface/ti-icdi.cfg -c "transport select jtag" -c "set WORKAREASIZE 0x8000" '
                     '-c "set CHIPNAME tm4c123gh6pm" -f target/ti/stellaris.cfg -c "reset_config none" '
                     '-c "tm4c123gh6pm.cpu configure -event reset-assert-post {sleep 100}"')
        seen = set()
        for path, board in roster_flashers():
            if 'flasher_recover' not in board:
                continue
            prim, name = board['flasher'], board['name']
            seen.add(name)
            self.assertNotIn('uid', board['flasher_recover'], f'{name}: the probe is the primary\'s')
            rec = hil_flash.recover_flasher(board)
            self.assertEqual(rec['name'], 'openocd', name)
            self.assertEqual(rec['uid'], prim['uid'], name)
            self.assertTrue(hil_flash.convoy_safe(rec), name)
            if prim['name'] == 'stlink':
                self.assertEqual(rec['args'], f'-f interface/stlink.cfg -f target/{stlink_targets[name]}.cfg')
            elif prim['name'] == 'lm4flash':
                self.assertEqual(rec['args'], tm4c_args)
            else:
                self.assertEqual(prim['name'], 'jlink', name)
                self.assertIn('interface/jlink.cfg', rec['args'], name)
                self.assertIn('transport select swd', rec['args'], name)
                stm32 = re.search(r'-f target/(stm32(?:f0|f4|f7|l4)x)\.cfg', rec['args'])
                if name in reset_only:
                    self.assertEqual(rec['args'], reset_only[name])
                elif stm32:
                    override = f'-c "{stm32.group(1)}.cpu configure -event reset-init {{}}"'
                    self.assertIn(override, rec['args'], name)
                    self.assertGreater(rec['args'].index(override), stm32.start(), name)
                    self.assertNotIn('adapter speed', rec['args'], name)
                else:
                    self.assertIn('adapter speed', rec['args'], name)
                if name not in reset_only:
                    self.assertIn('-f target/', rec['args'], name)
            for fn in (f'flash_{rec["name"]}', f'reset_{rec["name"]}'):
                self.assertTrue(callable(getattr(hil_flash, fn, None)), f'{name}: {fn}')
            if hil_flash.reset_primitive(rec['name']) is None:   # recover_hang reflashes only then
                self.assertEqual(hil_flash.FLASHER_SUFFIX[rec['name']], hil_flash.FLASHER_SUFFIX[prim['name']],
                                 f'{name}: the recovery reflashes the artifact the primary flashed')
        self.assertEqual(seen, demonstrated)


class TestUsbtestRecoveryCoverage(unittest.TestCase):
    """A usbtest battery that hangs is recovered only through a convoy-safe recovery
    flasher (usbtest.recover_hang); every other board stays wedged for the rest of the run.
    So every board the roster runs usbtest on must have one."""

    # not yet demonstrated on hardware; drop a board once its recovery flasher is proven
    PENDING = {'tinyusb.json': set(), 'hfp.json': set()}

    def test_every_usbtest_board_can_be_recovered(self):
        uncovered = {}
        for name in self.PENDING:
            with open(os.path.join(REPO, 'test/hil', name)) as f:
                boards = json.load(f)['boards']
            uncovered[name] = {b['name'] for b in boards if 'device/usbtest' in change_impact.board_tests(b)
                               and not hil_flash.convoy_safe(hil_flash.recover_flasher(b))}
        self.assertEqual(uncovered, self.PENDING)


if __name__ == '__main__':
    unittest.main()
