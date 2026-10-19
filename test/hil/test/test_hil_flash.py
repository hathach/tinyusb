#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# hil_flash's board-shaped adapters over agentrc's flash provider (lib/agentrc at the pin,
# hil_util.provider()), and the rosters checked by the provider's rules. The provider's own
# commands, scripts, retries and refusals are agentrc's tests (tests/test_hil_flash.py); these
# pin what tinyusb hands it (routing, bounds, runner and its own settings) and that a missing,
# off-pin or incomplete provider is refused before any tool runs. Run directly:
#   python3 test/hil/test/test_hil_flash.py
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

HIL_DIR = Path(__file__).resolve().parents[1]
REPO = str(HIL_DIR.parents[1])
sys.path.insert(0, str(HIL_DIR))
sys.path.insert(0, os.path.join(REPO, 'tools'))
import change_impact  # noqa: E402
import hil_flash  # noqa: E402
from helper import agentrc, hil_util  # noqa: E402
from test_change_impact import flasher_names, roster_flashers  # noqa: E402

PROVIDER_DIR = 'skills/hil/scripts'
PROVIDER_FILES = ('hil_flash.py', 'hil_process.py', 'pyocd/run_pyocd.py', 'pyocd/user_script.py')
BOARDS = {
    'jlink': {'name': 'jlink', 'uid': 'S1', 'args': '-device x'},
    'stlink': {'name': 'stlink', 'uid': 'S1'},
    'openocd': {'name': 'openocd', 'uid': 'S1', 'vid_pid': '0x2e8a 0x000c',
                'args': '-f interface/cmsis-dap.cfg -f target/rp2040.cfg'},
    'lm4flash': {'name': 'lm4flash', 'uid': 'S1', 'args': ''},
    'pyocd': {'name': 'pyocd', 'uid': 'S1', 'vid_pid': '0x1fc9 0x0090', 'args': '-t lpc55s28'},
    'esptool': {'name': 'esptool', 'uid': 'S1', 'args': '--baud 921600'},
}


class Provider:
    """hil_util.provider() stand-in recording each flash/reset call as (name, args, kwargs) and
    answering rc 0: what tinyusb hands the provider, not what the provider does with it."""

    def __init__(self, case):
        self.calls, self.real = [], hil_util.provider()
        case.addCleanup(setattr, hil_util, 'provider', hil_util.provider)
        hil_util.provider = lambda: self

    def __getattr__(self, name):
        if not name.startswith(('flash_', 'reset_', 'rescue_')):
            return getattr(self.real, name)

        def call(*args, **kwargs):
            self.calls.append((name, args, kwargs))
            return subprocess.CompletedProcess(name, 0, '', '')
        return call


class Adapters(unittest.TestCase):
    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        fw_dir = Path(td.name) / 'esp'
        fw_dir.mkdir()
        (fw_dir / 'config.env').write_text(json.dumps({'IDF_TARGET': 'esp32s3'}))
        (fw_dir / 'flash_args').write_text('--flash_mode dio\n0x0 bootloader.bin\n0x10000 fw.bin\n')
        self.esp_fw = str(fw_dir / 'fw.bin')
        patcher = mock.patch.object(hil_util, 'get_serial_dev', lambda *a: '/dev/ttyESP')
        patcher.start()
        self.addCleanup(patcher.stop)
        self.provider = Provider(self)

    def board(self, name):
        return {'name': 'b', 'flasher': dict(BOARDS[name])}

    def fw(self, name):
        return self.esp_fw if name == 'esptool' else '/tmp/fw.elf'

    def test_each_flasher_routes_to_its_provider_operation_with_the_callers_bound_and_runner(self):
        names = flasher_names()
        self.assertEqual(set(names), set(BOARDS))
        for name in names:
            for verb, fn, args in (('flash', hil_flash.flash_primitive(name), (self.fw(name),)),
                                   ('reset', hil_flash.reset_primitive(name), ())):
                if fn is None:
                    continue
                with self.subTest(fn=fn.__name__):
                    self.provider.calls.clear()
                    fn(self.board(name), *args, timeout=7)
                    (called, pargs, kw), = self.provider.calls
                    self.assertEqual((called, pargs[0]), (f'{verb}_{name}', BOARDS[name]))
                    self.assertEqual((kw['timeout'], kw['run']), (7, hil_util.run_cmd))

    def test_without_a_bound_cmd_timeout_is_read_at_call_time(self):
        self.addCleanup(setattr, hil_util, 'CMD_TIMEOUT', hil_util.CMD_TIMEOUT)
        hil_util.CMD_TIMEOUT = 33
        hil_flash.reset_stlink(self.board('stlink'))
        hil_flash.flash_jlink(self.board('jlink'), '/tmp/fw.elf')
        self.assertEqual([kw['timeout'] for _, _, kw in self.provider.calls], [33, 33])

    def test_jlink_reruns_a_crash_twice(self):
        hil_flash.flash_jlink(self.board('jlink'), '/tmp/fw.elf')
        hil_flash.reset_jlink(self.board('jlink'))
        self.assertEqual([kw['crash_retries'] for _, _, kw in self.provider.calls], [2, 2])

    def test_esptool_reads_the_builds_target_and_flash_args_and_runs_there(self):
        hil_flash.flash_esptool(self.board('esptool'), self.esp_fw, timeout=7)
        (_, _, kw), = self.provider.calls
        self.assertEqual({k: kw[k] for k in ('chip', 'port', 'flash_args', 'cwd')},
                         {'chip': 'esp32s3', 'port': '/dev/ttyESP', 'cwd': str(Path(self.esp_fw).parent),
                          'flash_args': '--flash_mode dio 0x0 bootloader.bin 0x10000 fw.bin'})

    def test_pyocd_runs_with_this_project_dir(self):
        hil_flash.flash_pyocd(self.board('pyocd'), 'rel/fw.elf')
        hil_flash.reset_pyocd(self.board('pyocd'))
        self.assertEqual([kw['project_dir'] for _, _, kw in self.provider.calls],
                         [str(hil_flash.PYOCD_PROJECT_DIR)] * 2)

    def test_rescue_hands_the_flash_output_over_with_the_bound_and_runner(self):
        hil_flash.rescue_openocd(self.board('openocd'), 'flash output')
        (name, args, kw), = self.provider.calls
        self.assertEqual((name, args), ('rescue_openocd', (BOARDS['openocd'], 'flash output')))
        self.assertEqual(kw, {'timeout': hil_util.CMD_TIMEOUT, 'run': hil_util.run_cmd})


def hil_test_stdout(case, board: dict, **env) -> str:
    """hil_test.py's stdout for a one-board config it must refuse (exit 1) before any board access."""
    with tempfile.TemporaryDirectory() as td:
        cfg = Path(td) / 'c.json'
        cfg.write_text(json.dumps({'boards': [{'name': 'b', 'uid': 'X', 'tests': {'device': True}, **board}]}))
        r = subprocess.run([sys.executable, str(HIL_DIR / 'hil_test.py'), str(cfg)],
                           env={**os.environ, 'HIL_REPORT_DIR': td, **env}, capture_output=True, text=True, timeout=60)
    case.assertEqual(r.returncode, 1, r.stderr)
    return r.stdout


class HilTestRefusesAMalformedPin(unittest.TestCase):
    def test_before_touching_a_board(self):
        jlink = {'name': 'jlink', 'uid': 'S', 'args': '-device x'}
        bad = {'name': 'openocd', 'vid_pid': '0x1a86 0x8010 0x1a86', 'args': '-f interface/jlink.cfg'}
        for board in ({'flasher': {**bad, 'uid': 'S'}}, {'flasher': jlink, 'flasher_recover': bad}):
            with self.subTest(board=board):
                self.assertIn('ERROR: an openocd "vid_pid" must be complete "0xVVVV 0xPPPP" pairs: b',
                              hil_test_stdout(self, board))


class ProviderLoading(unittest.TestCase):
    """hil_util.provider() loads agentrc's hil_flash once per process, with its runner as
    `.hil_process`, and only on first use."""

    def test_one_pinned_instance_whose_runner_hil_util_wraps(self):
        p = hil_util.provider()
        self.assertIs(hil_util.provider(), p)
        scripts = Path(REPO) / agentrc.DEP / PROVIDER_DIR
        self.assertEqual(Path(p.__file__).resolve(), (scripts / 'hil_flash.py').resolve())
        self.assertEqual(Path(p.hil_process.__file__).resolve(), (scripts / 'hil_process.py').resolve())
        seen = []
        with mock.patch.object(p.hil_process, 'run_cmd', lambda cmd, cwd, **kw: seen.append(cmd)):
            hil_util.run_cmd('true')
        self.assertEqual(seen, ['true'])

    def scratch(self, provider_files):
        """A tree holding the selector, the harness's bottom layer and hil_flash, and lib/agentrc
        at the pin with `provider_files` (None: no lib/agentrc at all)."""
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        root = Path(td.name)
        for rel in [*(f'tools/{p.name}' for p in Path(REPO, 'tools').glob('*.py')),
                    *(f'test/hil/helper/{p.name}' for p in (HIL_DIR / 'helper').glob('*.py')),
                    'test/hil/hil_flash.py']:
            (root / rel).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(Path(REPO, rel), root / rel)
        if provider_files is not None:
            for f in provider_files:
                rel = Path(agentrc.DEP, PROVIDER_DIR, f)
                (root / rel).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy(Path(REPO, rel), root / rel)
            (root / agentrc.DEP / '.git').mkdir(parents=True)
            (root / agentrc.DEP / '.git/HEAD').write_text(agentrc.PIN + '\n')
        return root

    def python(self, root, code):
        return subprocess.run([sys.executable, '-c', 'import sys; sys.path[:0] = ["test/hil", "tools"]\n' + code],
                              cwd=root, capture_output=True, text=True, timeout=60)

    def test_selection_runs_without_lib_agentrc(self):
        r = self.python(self.scratch(None), (
            'import change_impact, hil_flash\n'
            'assert hil_flash.flash_primitive("JLink") is hil_flash.flash_jlink\n'
            'assert hil_flash.find_firmware("b", "device/cdc_msc", flasher="jlink") is None\n'
            'assert change_impact.ALL_TESTS["device"]\n'
            'print(sorted(m for m in sys.modules if "provider" in m or m.startswith("agentrc")))\n'))
        self.assertEqual((r.returncode, r.stdout), (0, '[]\n'), r.stderr)

    def test_a_missing_off_pin_or_incomplete_provider_refuses_before_any_tool_runs(self):
        code = ('import hil_flash\n'
                'from helper import hil_util\n'
                'for call in (hil_util.provider, lambda: hil_util.run_cmd("touch ran"),\n'
                '             lambda: hil_flash.reset_stlink({"flasher": {"name": "stlink", "uid": "S"}})):\n'
                '    try:\n'
                '        call()\n'
                '    except ImportError as e:\n'
                '        print(e)\n')
        cases = [(None, f'must be at {agentrc.PIN} (found nothing)')]
        for missing in PROVIDER_FILES:
            cases.append(([f for f in PROVIDER_FILES if f != missing], f'has no {PROVIDER_DIR}/{missing}'))
        off_pin = self.scratch(PROVIDER_FILES)
        (off_pin / agentrc.DEP / '.git/HEAD').write_text('0' * 40 + '\n')
        for root, why in [(self.scratch(files), why) for files, why in cases] + [
                (off_pin, f'must be at {agentrc.PIN} (found {"0" * 40})')]:
            with self.subTest(why=why):
                r = self.python(root, code)
                self.assertEqual(r.returncode, 0, r.stderr)
                lines = r.stdout.splitlines()
                self.assertEqual(len(lines), 3, r.stdout)
                for line in lines:
                    self.assertIn(why, line)
                    self.assertIn(agentrc.FETCH, line)
                self.assertFalse((root / 'ran').exists())

    def test_a_complete_provider_at_the_pin_loads(self):
        r = self.python(self.scratch(PROVIDER_FILES), 'from helper import hil_util\nprint(hil_util.run_cmd("printf ok").stdout)\n')
        self.assertEqual((r.returncode, r.stdout), (0, 'ok\n'), r.stderr)


class Rosters(unittest.TestCase):
    """The committed rosters, checked by the provider's rules."""

    def test_roster_vid_pid_pins_are_complete_pairs(self):
        # the provider refuses every openocd command over a malformed explicit pin
        for path, board in roster_flashers():
            for f in (board['flasher'], hil_flash.recover_flasher(board)):
                if 'vid_pid' in f:
                    self.assertTrue(hil_flash.valid_vid_pid(f['vid_pid']), f"{path}: {board['name']}")

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
