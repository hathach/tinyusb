"""hil_recover: the post-pool wedge recovery. Every privileged or hardware step is a fake
(the shield script, the holder scan, the flasher primitives, the USB scan), the lock dir is
a temp dir, and the assertions are about ORDER and GATES: the fleet reservation before any
shield, unshield in a finally, the marker cleared only on a complete no-holder scan plus a
re-enumerated DUT, everything deferred with the marker kept otherwise."""
import io
import json
import os
import subprocess
import sys
import tempfile
import time
import types
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE.parent / 'helper'))
import usbtest                              # noqa: E402
import hil_flash                            # noqa: E402
from helper import hil_health, hil_lock, hil_recover, hil_report, hil_util   # noqa: E402

CFG = {'boards': [
    {'name': 'b1', 'uid': 'UID1', 'flasher': {'name': 'jlink', 'uid': 'P1', 'args': '-device x'},
     # openocd over stlink without a pin is NOT convoy-safe: b1 is the shielded board,
     # b2 (pinned by vid_pid) the one recovered without a shield
     'flasher_recover': {'name': 'openocd', 'args': '-f interface/stlink.cfg -f target/x.cfg'}},
    {'name': 'b2', 'uid': 'UID2', 'flasher': {'name': 'openocd', 'uid': 'P2', 'vid_pid': '0x2e8a 0x000c', 'args': '-f t.cfg'}},
], 'boards-skip': [{'name': 'parked', 'uid': 'UID3', 'flasher': {'name': 'jlink', 'uid': 'P3', 'args': ''}}]}
NODE = '/dev/bus/usb/013/042'
REAL_PRECONDITIONS = hil_recover.preconditions     # captured before any test patches them
REAL_SHIELD_PRECONDITIONS = hil_recover.shield_preconditions
REAL_BUSPORT_OF_NODE = hil_recover.busport_of_node


class Recovery(unittest.TestCase):
    def setUp(self):
        td = self.td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.patch(hil_lock, 'BOARD_LOCK_DIR', td.name)
        self.fw = Path(td.name) / 'fw.elf'
        self.fw.write_bytes(b'elf')
        self.calls = []
        self.scans = [([], True)]                  # what wedged_pids returns, in order
        self.script_rc = {'shield': 0, 'unshield': 0}
        self.enumerated = [{'busport': '13-2.3', 'dir': '', 'vid': 'cafe', 'pid': '4010', 'serial': 'UID1'}]
        self.busport = '13-2.3'
        self.patch(hil_recover, 'preconditions', lambda skip_flash=False: '')
        self.shield_why = ''
        self.shield_checks = 0
        self.patch(hil_recover, 'shield_preconditions', self.fake_shield_preconditions)
        self.patch(hil_recover, 'busport_of_node', lambda node: self.busport)
        self.patch(hil_recover, 'SETTLE', 0)
        self.patch(hil_recover, '_script', self.fake_script)
        # the real usb_scan matches `serial` case-insensitively
        self.patch(hil_util, 'usb_scan', lambda serial=None, **kw: [
            d for d in self.enumerated if serial is None or d['serial'].lower() == serial.lower()])
        self.patch(usbtest, 'wedged_pids', self.fake_scan)
        self.patch(hil_flash, 'reset_openocd', lambda board, timeout=None: self.calls.append(('reset', 'openocd', timeout)) or subprocess.CompletedProcess('', 0))
        self.patch(hil_flash, 'flash_openocd', lambda board, fw, timeout=None: self.calls.append(('flash', fw, timeout)) or subprocess.CompletedProcess('', 0))
        self.log = []

    def patch(self, obj, name, value):
        self.addCleanup(setattr, obj, name, getattr(obj, name))
        setattr(obj, name, value)

    def fake_shield_preconditions(self):
        self.shield_checks += 1
        return self.shield_why

    def mark_b2(self):
        self.mark('b2', uid='UID2', evidence={'node': NODE, 'serial': 'UID2', 'holders': [4242], 'complete': True})
        self.enumerated.append({'busport': '13-2.4', 'dir': '', 'vid': 'cafe', 'pid': '4010', 'serial': 'UID2'})

    def fake_script(self, action, *args, timeout=60):
        self.calls.append((action, *args))
        return self.script_rc.get(action, 0), f'{action} stub'

    def fake_scan(self, node):
        self.calls.append(('scan', node))
        return self.scans.pop(0) if len(self.scans) > 1 else self.scans[0]

    def mark(self, board='b1', **over):
        fh = hil_lock.flock_nb(board)
        info = {'uid': 'UID1', 'reason': 'wedged', 'confirmation': 'confirmed',
                'evidence': {'node': NODE, 'serial': 'UID1', 'holders': [4242], 'complete': True},
                'fw': str(self.fw), **over}
        self.assertTrue(hil_lock.write_wedged(board, info, fh))
        hil_lock.clear_record(fh)
        fh.close()

    def run_phase(self, boards=None, **kw):
        kw.setdefault('supervise', False)        # in-process, so the fakes' call log is ours to read
        return hil_recover.recover_wedged(CFG, boards or CFG['boards'], self.log.append, **kw)

    def test_nothing_marked_does_nothing(self):
        self.assertEqual(self.run_phase(), {})
        self.assertEqual(self.calls, [])

    def test_reset_clears_the_holder_and_the_marker_is_cleared_with_evidence(self):
        self.mark()
        out = self.run_phase()
        self.assertTrue(out['b1']['recovered'], out)
        self.assertEqual(out['b1']['identity'], 'UID1@13-2.3')
        kinds = [c[0] for c in self.calls]
        # shield before the reset, the scan after it, unshield before the marker goes
        self.assertEqual(kinds, ['shield', 'reset', 'scan', 'unshield'])
        self.assertEqual(self.calls[0], ('shield', '13-2.3', str(os.getpid())))
        self.assertEqual(self.calls[1], ('reset', 'openocd', usbtest.RECOVER_RESET_TIMEOUT))
        self.assertIsNone(hil_lock.read_wedged('b1'))
        # the reservation was released: every board's flock is free again
        for name in ('b1', 'b2', 'parked'):
            hil_lock.flock_nb(name).close()

    def test_a_surviving_holder_gets_the_reflash_then_the_verdict(self):
        self.mark()
        self.scans = [([4242], True), ([], True)]
        out = self.run_phase()
        self.assertTrue(out['b1']['recovered'], out)
        kinds = [c[0] for c in self.calls]
        self.assertEqual(kinds, ['shield', 'reset', 'scan', 'flash', 'scan', 'unshield'])
        self.assertEqual(self.calls[3], ('flash', str(self.fw), usbtest.RECOVER_FLASH_TIMEOUT))

    def test_a_holder_that_survives_everything_keeps_the_marker(self):
        self.mark()
        self.scans = [([4242], True)]
        out = self.run_phase()
        self.assertFalse(out['b1']['recovered'])
        self.assertEqual(out['b1']['why'], 'a D-state holder survived the reset and the reflash')
        self.assertEqual([c[0] for c in self.calls][-1], 'unshield')
        self.assertIsNotNone(hil_lock.read_wedged('b1'))

    def test_an_incomplete_scan_is_not_a_recovery(self):
        self.mark()
        self.scans = [([], False)]
        out = self.run_phase()
        self.assertFalse(out['b1']['recovered'])
        self.assertEqual(out['b1']['why'], 'holder scan incomplete after the reset and the reflash')
        self.assertIsNotNone(hil_lock.read_wedged('b1'))

    def test_a_reflash_that_raises_is_not_worded_as_run(self):
        self.mark()
        self.scans = [([4242], True)]

        def raising_flash(board, fw, timeout=None):
            raise RuntimeError('probe gone')
        self.patch(hil_flash, 'flash_openocd', raising_flash)
        out = self.run_phase()
        self.assertFalse(out['b1']['recovered'])
        self.assertEqual(out['b1']['why'], 'a D-state holder survived the reset and a reflash that raised')
        self.assertTrue(any('raised RuntimeError: probe gone' in s for s in out['b1']['steps']), out)

    def test_a_reset_that_raises_is_not_worded_as_run(self):
        self.mark()
        self.scans = [([4242], True)]

        def raising_reset(board, timeout=None):
            raise RuntimeError('probe gone')
        self.patch(hil_flash, 'reset_openocd', raising_reset)
        out = self.run_phase()
        self.assertFalse(out['b1']['recovered'])
        self.assertEqual(out['b1']['why'], 'a D-state holder survived a reset that raised and the reflash')
        self.assertTrue(any('probe reset via openocd raised RuntimeError: probe gone' in s
                            for s in out['b1']['steps']), out)

    def test_a_reset_or_reflash_that_failed_is_not_worded_as_run(self):
        """run_cmd returns rc 124 on a timeout rather than raising."""
        self.mark()
        self.scans = [([4242], True)]
        self.patch(hil_flash, 'reset_openocd', lambda board, timeout=None: subprocess.CompletedProcess('', 124))
        out = self.run_phase()
        self.assertFalse(out['b1']['recovered'])
        self.assertEqual(out['b1']['why'], 'a D-state holder survived a reset that failed (rc 124) and the reflash')
        self.assertIn('probe reset via openocd: rc 124', out['b1']['steps'])
        self.patch(hil_flash, 'flash_openocd', lambda board, fw, timeout=None: subprocess.CompletedProcess('', 1))
        out = self.run_phase()
        self.assertEqual(out['b1']['why'],
                         'a D-state holder survived a reset that failed (rc 124) and a reflash that failed (rc 1)')
        self.assertIn('reflash via openocd: rc 1', out['b1']['steps'])

    def test_no_reset_primitive_goes_straight_to_the_reflash(self):
        self.mark()
        self.patch(usbtest, 'reset_primitive', lambda name: None)
        self.scans = [([4242], True)]
        out = self.run_phase()
        self.assertFalse(out['b1']['recovered'])
        self.assertEqual(out['b1']['why'], 'a D-state holder survived the reflash')
        self.assertEqual([c[0] for c in self.calls], ['shield', 'flash', 'scan', 'unshield'])

    def test_no_reset_primitive_and_no_reflash_scans_nothing(self):
        self.mark(fw='')
        self.patch(usbtest, 'reset_primitive', lambda name: None)
        out = self.run_phase()
        self.assertFalse(out['b1']['recovered'])
        self.assertEqual(out['b1']['why'],
                         'holder not re-scanned; no reset or reflash ran; reflash skipped: no firmware artifact recorded')
        self.assertEqual([c[0] for c in self.calls], ['shield', 'unshield'])
        self.assertIsNotNone(hil_lock.read_wedged('b1'))

    def test_a_failed_unshield_keeps_the_marker_even_after_a_clean_scan(self):
        self.mark()
        self.script_rc['unshield'] = 1
        out = self.run_phase()
        self.assertFalse(out['b1']['recovered'])
        self.assertIn('unshield failed, shield record kept', out['b1']['why'])
        self.assertIsNotNone(hil_lock.read_wedged('b1'))

    def test_a_refused_shield_touches_no_probe(self):
        self.mark()
        self.script_rc['shield'] = 1
        out = self.run_phase()
        self.assertFalse(out['b1']['recovered'])
        self.assertEqual([c[0] for c in self.calls], ['shield'], 'a refusal published nothing of ours')

    def test_the_dut_must_re_enumerate_exactly_once(self):
        self.mark()
        self.enumerated = []
        out = self.run_phase()
        self.assertFalse(out['b1']['recovered'])
        self.assertIn('not enumerated exactly once (0 found)', out['b1']['why'])
        self.assertIsNotNone(hil_lock.read_wedged('b1'))

    def test_a_held_board_anywhere_in_the_fleet_defers_the_whole_phase(self):
        self.mark()
        holder = hil_lock.flock_nb('parked')          # a parked board, not even the wedged one
        self.addCleanup(holder.close)
        hil_lock.write_record(holder, 'someone')
        out = self.run_phase()
        self.assertEqual(out, {})
        self.assertEqual(self.calls, [])
        self.assertTrue(any('fleet reservation refused (parked is held by someone' in l for l in self.log), self.log)
        self.assertIsNotNone(hil_lock.read_wedged('b1'))
        hil_lock.flock_nb('b1').close()               # nothing left held by the refused attempt

    def test_a_shielded_board_without_shield_prerequisites_keeps_its_marker_untouched(self):
        self.mark()
        for why in ('running as root: the shield does not apply', 'no passwordless sudo',
                    'usb_recover.sh missing (a staged tree carries no .claude)'):
            self.shield_why = why
            out = self.run_phase()
            self.assertFalse(out['b1']['recovered'])
            self.assertIn(why, out['b1']['why'])
        self.assertEqual(self.calls, [])              # no shield, no probe, no scan
        self.assertIsNotNone(hil_lock.read_wedged('b1'))

    def test_a_convoy_safe_board_recovers_without_a_shield(self):
        self.mark_b2()
        out = self.run_phase()
        self.assertTrue(out['b2']['recovered'], out)
        self.assertEqual([c[0] for c in self.calls], ['reset', 'scan'])
        self.assertTrue(any('no shield' in s for s in out['b2']['steps']), out)
        self.assertEqual(self.shield_checks, 0)       # never asks for root-free sudo
        self.assertIsNone(hil_lock.read_wedged('b2'))

    def test_a_convoy_safe_board_recovers_as_root_without_sudo_or_the_script(self):
        self.mark_b2()
        self.shield_why = 'running as root: the shield does not apply'
        self.patch(hil_util, 'run_cmd', lambda *a, **k: self.fail(f'sudo invoked: {a}'))
        self.assertTrue(self.run_phase()['b2']['recovered'])

    def test_a_mixed_set_recovers_the_safe_board_and_keeps_the_shielded_one(self):
        self.mark()
        self.mark_b2()
        self.shield_why = 'no passwordless sudo for the shield'
        out = self.run_phase()
        self.assertTrue(out['b2']['recovered'], out)
        self.assertFalse(out['b1']['recovered'])
        self.assertIsNotNone(hil_lock.read_wedged('b1'))
        self.assertIsNone(hil_lock.read_wedged('b2'))

    def test_a_shielded_board_whose_node_is_gone_needs_no_shield_prerequisites(self):
        self.mark()
        self.busport = ''
        self.shield_why = 'no passwordless sudo for the shield'
        self.assertTrue(self.run_phase()['b1']['recovered'])
        self.assertEqual(self.shield_checks, 0)

    def test_the_shield_steps_are_budgeted_only_for_a_shielded_board(self):
        ut = hil_recover._usbtest()
        safe = (hil_recover.step_cost(ut.RECOVER_RESET_TIMEOUT) + hil_recover.SETTLE
                + 2 * hil_recover.SCAN_ALLOWANCE)
        shielded = safe + 2 * hil_recover.step_cost(hil_recover.SHIELD_TIMEOUT)
        self.patch(hil_recover, 'PHASE_TIMEOUT', (safe + shielded) // 2)
        self.mark_b2()
        self.assertTrue(self.run_phase()['b2']['recovered'])
        self.mark()
        out = self.run_phase()
        self.assertIn('budget exhausted', out['b1']['why'])
        self.assertEqual([c for c in self.calls if c[0] == 'shield'], [])

    def test_real_preconditions(self):
        self.assertEqual(REAL_PRECONDITIONS(), '')
        self.assertIn('--skip-flash', REAL_PRECONDITIONS(skip_flash=True))
        real = REAL_SHIELD_PRECONDITIONS
        self.patch(os, 'geteuid', lambda: 0)
        self.assertIn('root', real())
        self.patch(os, 'geteuid', lambda: 1000)
        self.patch(hil_util, 'run_cmd', lambda *a, **k: subprocess.CompletedProcess('', 1))
        self.assertIn('sudo', real())
        self.patch(hil_util, 'run_cmd', lambda *a, **k: subprocess.CompletedProcess('', 0))
        self.assertEqual(real(), '')

    def test_a_marker_without_a_node_is_left_alone(self):
        self.mark(evidence={'serial': 'UID1'})
        out = self.run_phase()
        self.assertIn('no device node', out['b1']['why'])
        self.assertEqual(self.calls, [])
        self.assertIsNotNone(hil_lock.read_wedged('b1'))

    def test_a_device_already_gone_from_its_node_needs_only_the_scans(self):
        self.mark()
        self.busport = ''
        out = self.run_phase()
        self.assertTrue(out['b1']['recovered'], out)
        self.assertEqual([c[0] for c in self.calls], ['scan'])

    def test_a_reset_only_recovery_flasher_never_reflashes(self):
        CFG['boards'][0]['flasher_recover']['reflash'] = False
        self.addCleanup(CFG['boards'][0]['flasher_recover'].pop, 'reflash')
        self.mark()
        self.scans = [([4242], True)]
        out = self.run_phase()
        self.assertFalse(out['b1']['recovered'])
        self.assertNotIn('flash', [c[0] for c in self.calls])
        self.assertTrue(any('reset-only' in s for s in out['b1']['steps']), out)
        self.assertEqual(out['b1']['why'], 'a D-state holder survived the reset; reflash skipped: reset-only recovery flasher')

    def test_no_recorded_firmware_skips_the_reflash(self):
        self.mark(fw='')
        self.scans = [([4242], True)]
        out = self.run_phase()
        self.assertFalse(out['b1']['recovered'])
        self.assertNotIn('flash', [c[0] for c in self.calls])
        self.assertTrue(any('reflash skipped' in s for s in out['b1']['steps']))

    def test_a_board_name_with_a_trailing_newline_is_refused(self):
        with self.assertRaises(ValueError):
            hil_lock.wedged_path('b1\n')
        Path(self.td.name, 'b1\n' + hil_lock.WEDGED_SUFFIX).write_text('{}')
        [(board, info)] = hil_lock.wedged_boards()
        self.assertEqual(board, 'b1\n')
        self.assertIn('invalid name', info['reason'])

    def test_an_invalid_name_marker_is_listed_on_one_line_and_left_to_a_human(self):
        marker = Path(self.td.name, 'b1\n' + hil_lock.WEDGED_SUFFIX)
        marker.write_text('{}')
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out):
            self.assertEqual(hil_lock.cmd_wedged('status'), 0)
        self.assertEqual(out.getvalue().count('\n'), 1, out.getvalue())
        self.assertTrue(out.getvalue().startswith('"b1\\n": '), out.getvalue())
        self.assertIn('inspect and remove it by hand', out.getvalue())
        with redirect_stderr(err):
            self.assertEqual(hil_lock.cmd_wedged('clear', 'b1\n', '{}'), 1)
        self.assertEqual(err.getvalue().count('\n'), 1, err.getvalue())
        self.assertIn('removed by hand', err.getvalue())
        self.assertTrue(marker.exists())

    def test_the_budget_stops_the_phase_before_a_board_it_cannot_finish(self):
        self.mark()
        self.patch(hil_recover, 'PHASE_TIMEOUT', 1)
        out = self.run_phase()
        self.assertFalse(out['b1']['recovered'])
        self.assertIn('budget exhausted', out['b1']['why'])
        self.assertEqual(self.calls, [])

    def test_the_reservation_is_released_when_a_board_raises(self):
        self.mark()
        self.patch(hil_recover, 'recover_board', lambda *a, **k: 1 / 0)
        with self.assertRaises(ZeroDivisionError):
            self.run_phase()
        hil_lock.flock_nb('b1').close()

    def test_skip_flash_defers_the_whole_phase(self):
        self.mark()
        self.patch(hil_recover, 'preconditions', REAL_PRECONDITIONS)
        self.assertEqual(self.run_phase(skip_flash=True), {})
        self.assertEqual(self.calls, [])
        self.assertTrue(any('--skip-flash' in l and 'markers kept' in l for l in self.log), self.log)

    def test_the_marker_is_re_read_under_the_reservation(self):
        self.mark()
        real = hil_recover.reserve_all

        def clearing_reserve(config):
            os.unlink(hil_lock.wedged_path('b1'))       # an operator cleared it meanwhile
            return real(config)
        self.patch(hil_recover, 'reserve_all', clearing_reserve)
        self.assertEqual(self.run_phase(), {})
        self.assertEqual(self.calls, [])
        self.assertTrue(any('marker gone before the fleet was reserved' in l for l in self.log), self.log)

    def test_a_marker_for_other_hardware_is_not_acted_on(self):
        self.mark(uid='SOMEONE-ELSE')
        out = self.run_phase()
        self.assertIn("is not the roster's 'UID1'", out['b1']['why'])
        self.assertEqual(self.calls, [])
        self.assertIsNotNone(hil_lock.read_wedged('b1'))

    def test_an_own_shield_left_half_done_is_unshielded(self):
        self.mark()
        self.script_rc['shield'] = 1
        self.patch(hil_recover, '_script', lambda action, *a, timeout=60: (self.calls.append((action, *a)) or
                   ((1, 'shield: chmod 000 x failed; rollback incomplete: y; record r KEPT for unshield')
                    if action == 'shield' else (0, 'unshielded'))))
        out = self.run_phase()
        self.assertFalse(out['b1']['recovered'])
        self.assertEqual([c[0] for c in self.calls], ['shield', 'unshield'])
        # a foreign owner's refusal is not ours to undo
        self.calls.clear()
        self.patch(hil_recover, '_script', lambda action, *a, timeout=60: (self.calls.append((action, *a)) or
                   (1, 'shield: usb13 is already covered by the shield on 13-1.7 (owner 5 1 b alive); refusing')))
        self.mark()
        out = self.run_phase()
        self.assertEqual([c[0] for c in self.calls], ['shield'])
        self.assertIn('shield refused', out['b1']['why'])

    def test_every_step_is_capped_to_the_budget_and_the_reflash_yields_to_the_unshield(self):
        self.mark()
        clock = [1000.0]
        self.patch(hil_recover, 'PHASE_TIMEOUT', 300)
        self.patch(hil_recover, 'Budget', lambda seconds, now=None: _FakeBudget(seconds, clock))
        seen = {}

        def slow_reset(board, timeout=None):
            seen['reset_timeout'] = timeout
            clock[0] += 200                                    # the reset ran long
        self.patch(hil_flash, 'reset_openocd', slow_reset)
        self.scans = [([4242], True)]
        out = self.run_phase()
        self.assertFalse(out['b1']['recovered'])
        # the reset's own bound (30) fits; it is capped to what leaves room for the unshield
        self.assertEqual(seen['reset_timeout'], usbtest.RECOVER_RESET_TIMEOUT)
        self.assertTrue(any('reflash skipped: not enough budget' in st for st in out['b1']['steps']), out)
        self.assertEqual([c[0] for c in self.calls][-1], 'unshield')

    def test_budget_caps(self):
        clock = [0.0]
        b = hil_recover.Budget(100, now=lambda: clock[0])
        self.assertEqual(b.cap(30), 30)
        self.assertEqual(b.cap(30, reserve=80), 20)
        clock[0] = 99.5
        self.assertEqual(b.cap(30, reserve=80), 1)          # never zero: the step still runs and reports

    def test_an_expired_budget_stops_every_candidate_including_scan_only_ones(self):
        self.mark()
        self.busport = ''                                    # the scan-only path
        self.patch(hil_recover, 'PHASE_TIMEOUT', 0)
        out = self.run_phase()
        self.assertIn('budget exhausted', out['b1']['why'])
        self.assertEqual(self.calls, [])
        self.assertIsNotNone(hil_lock.read_wedged('b1'))

    def test_the_supervisor_reports_its_outcomes_and_frees_the_fleet(self):
        """The default path: a forked child in its own session runs the phase and the
        outcomes come back over the pipe; the fakes are inherited by the fork, so only the
        returned record and the lock dir can be asserted here."""
        self.mark()
        out = self.run_phase(supervise=True)
        self.assertTrue(out['b1']['recovered'], out)
        self.assertEqual(out['b1']['identity'], 'UID1@13-2.3')
        self.assertIsNone(hil_lock.read_wedged('b1'))
        for name in ('b1', 'b2', 'parked'):
            hil_lock.flock_nb(name).close()

    def test_a_supervisor_that_does_not_report_is_abandoned_with_the_markers_standing(self):
        self.mark()
        self.patch(hil_recover, 'PHASE_TIMEOUT', 1)
        self.patch(hil_recover, 'overrun', lambda: 0)
        self.patch(hil_util, 'REAP_GRACE', 0)
        self.patch(hil_recover, 'SHIELD_TIMEOUT', 1)

        def deaf_phase(*a, **k):                              # a phase the watchdog cannot reach
            import signal as sig
            sig.signal(sig.SIGALRM, sig.SIG_IGN)
            time.sleep(30)
        self.patch(hil_recover, '_phase', deaf_phase)
        t0 = time.monotonic()
        out = self.run_phase(supervise=True)
        self.assertEqual(out, {})
        self.assertLess(time.monotonic() - t0, 15)
        # PHASE_TIMEOUT + overrun() + step_cost(SHIELD_TIMEOUT) + 5 + REAP_GRACE = 1 + 0 + 1 + 5 + 0
        self.assertTrue(any('did not report within 7s' in l for l in self.log), self.log)
        self.assertIsNotNone(hil_lock.read_wedged('b1'))

    def test_the_watchdog_still_unshields_and_says_so(self):
        """A step that stalls past the budget: the alarm raises into the phase, the
        shield's finally runs the unshield, the release runs, and the parent hears why."""
        self.mark()
        trace = Path(self.td.name) / 'trace'

        def tracing_script(action, *a, timeout=60):
            with open(trace, 'a') as f:
                f.write(action + '\n')
            return 0, f'{action} stub'
        self.patch(hil_recover, '_script', tracing_script)
        self.patch(usbtest, 'wedged_pids', lambda node: time.sleep(60))       # the stall
        # shrink every bound so the board is admitted and the watchdog fires in seconds
        self.patch(hil_recover, 'SHIELD_TIMEOUT', 1)
        self.patch(hil_recover, 'SCAN_ALLOWANCE', 0)
        self.patch(usbtest, 'RECOVER_RESET_TIMEOUT', 1)
        self.patch(hil_util, 'REAP_GRACE', 0)
        self.patch(hil_recover, 'PHASE_TIMEOUT', 4)
        self.patch(hil_recover, 'overrun', lambda: 0)
        t0 = time.monotonic()
        out = self.run_phase(supervise=True)
        self.assertLess(time.monotonic() - t0, 15)
        self.assertEqual(trace.read_text().split(), ['shield', 'unshield'])
        self.assertTrue(any('watchdog: phase exceeded' in l for l in self.log), self.log)
        self.assertEqual(out.get('b1', {}).get('recovered', False), False)
        self.assertIsNotNone(hil_lock.read_wedged('b1'))
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:                    # the release follows the unshield
            try:
                hil_lock.flock_nb('b1').close(); break
            except OSError:
                time.sleep(0.2)
        else:
            self.fail('the fleet stayed reserved after the watchdog')

    def test_a_surviving_child_keeps_the_fleet_reserved_after_the_report(self):
        self.mark()
        until = time.monotonic() + 3
        self.patch(hil_recover, '_sweep', lambda tracked, log: 1 if time.monotonic() < until else 0)
        out = self.run_phase(supervise=True)
        self.assertTrue(out['b1']['recovered'], out)            # the verdict arrived first
        self.assertTrue(any('survived' in l and 'keeps every board reserved' in l for l in self.log), self.log)
        with self.assertRaises(OSError):
            hil_lock.flock_nb('parked')                          # still held by the supervisor
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            try:
                hil_lock.flock_nb('parked').close(); break
            except OSError:
                time.sleep(0.5)
        else:
            self.fail('the supervisor never released after its children ended')

    def test_retention_outlives_the_watchdog(self):
        """A survivor that never dies: the alarms are cancelled on entering retention, so
        the supervisor keeps the fleet past the phase bound until it is killed itself."""
        self.mark()
        self.patch(hil_recover, '_sweep', lambda tracked, log: 1)
        self.patch(hil_recover, 'PHASE_TIMEOUT', 1)
        self.patch(hil_recover, 'overrun', lambda: 0)
        self.patch(hil_recover, 'SHIELD_TIMEOUT', 1)
        self.patch(hil_util, 'REAP_GRACE', 0)
        out = self.run_phase(supervise=True)
        self.assertIn('b1', out)                                 # the verdict arrived; retention follows it
        pid = int(next(l for l in self.log if 'keeps every board reserved' in l).split('(pid ')[1].split(')')[0])
        self.addCleanup(lambda: (os.kill(pid, 9) if os.path.exists(f'/proc/{pid}') else None))
        time.sleep(4)                                            # well past phase + cleanup alarms
        with self.assertRaises(OSError):
            hil_lock.flock_nb('parked')
        os.kill(pid, 9)
        time.sleep(0.5)
        hil_lock.flock_nb('parked').close()                      # the kernel dropped its flocks

    def test_sweep_tracks_identities_past_reparenting(self):
        """A step's grandchild reparented to init is no longer a descendant, but it is still
        the same identity: it is killed and counted until it is gone."""
        wrapper = subprocess.Popen(['bash', '-c', 'sleep 100 & echo $!; wait'], stdout=subprocess.PIPE, text=True)
        self.addCleanup(wrapper.kill)
        grandchild = int(wrapper.stdout.readline())
        self.addCleanup(lambda: (os.kill(grandchild, 9) if os.path.exists(f'/proc/{grandchild}') else None))
        tracked = hil_recover._descendants()
        self.assertIn(grandchild, tracked)
        wrapper.kill(); wrapper.wait()
        time.sleep(0.3)                                         # the sleep is now init's child
        self.assertFalse(any(grandchild in [p for p, _ in kids] for kids in
                             hil_health.child_procs([os.getpid()]).values()))
        self.assertEqual(hil_recover._sweep(tracked, self.log.append), 0)
        time.sleep(0.2)
        self.assertFalse(hil_recover._alive(grandchild, tracked[grandchild]))
        # a failed enumeration is a survivor, never zero
        self.assertEqual(hil_recover._sweep({'__unknown__': ''}, self.log.append), 1)

    def test_a_shield_interrupted_by_the_watchdog_is_still_unshielded(self):
        self.mark()
        trace = Path(self.td.name) / 'trace'

        def stalling_script(action, *a, timeout=60):
            with open(trace, 'a') as f:
                f.write(action + '\n')
            if action == 'shield':
                time.sleep(60)                                   # published, never returned
            return 0, f'{action} stub'
        self.patch(hil_recover, '_script', stalling_script)
        self.patch(hil_recover, 'SHIELD_TIMEOUT', 1)
        self.patch(hil_recover, 'SCAN_ALLOWANCE', 0)
        self.patch(usbtest, 'RECOVER_RESET_TIMEOUT', 1)
        self.patch(hil_util, 'REAP_GRACE', 0)
        self.patch(hil_recover, 'PHASE_TIMEOUT', 4)
        self.patch(hil_recover, 'overrun', lambda: 0)
        self.run_phase(supervise=True)
        self.assertEqual(trace.read_text().split(), ['shield', 'unshield'])
        self.assertIsNotNone(hil_lock.read_wedged('b1'))

    def test_a_supervisor_that_dies_reports_the_error(self):
        self.mark()
        self.patch(hil_recover, '_phase', lambda *a, **k: 1 / 0)
        out = self.run_phase(supervise=True)
        self.assertEqual(out, {})
        self.assertTrue(any('supervisor: ZeroDivisionError' in l for l in self.log), self.log)

    def test_busport_of_node_reads_lock_free_attributes(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        # a fake sysfs is not reachable through the real path, so exercise the parser and
        # the miss path against the real tree
        self.assertEqual(REAL_BUSPORT_OF_NODE('/dev/bus/usb/999/001'), '')
        self.assertEqual(REAL_BUSPORT_OF_NODE('garbage'), '')


class _FakeBudget(hil_recover.Budget):
    def __init__(self, seconds, clock):
        super().__init__(seconds, now=lambda: clock[0])


class RowsInHilTest(unittest.TestCase):
    """hil_test._recover_wedged_rows: the recovery outcome rewrites only the wedge cells of
    a recovered board, refused ones into their own recovered form, and --skip-flash reaches
    the helper."""
    def test_rows_and_flags(self):
        import hil_test
        seen = {}

        def fake(config, boards, log, skip_flash=False):
            seen['skip_flash'] = skip_flash
            return {'b1': {'recovered': True}, 'b2': {'recovered': False}}
        self.addCleanup(setattr, hil_recover, 'recover_wedged', hil_recover.recover_wedged)
        hil_recover.recover_wedged = fake
        self.addCleanup(setattr, hil_test, 'skip_flash', hil_test.skip_flash)
        hil_test.skip_flash = True
        W = hil_report.WEDGED_CELL
        mret = [('b1', 1, [], [('b1', {W: 'fail', 'device/usbtest': '❌ 10/30'}, None),
                              ('b1-dma', {W: hil_report.WEDGED_REFUSED}, None)], 0.0),
                ('b2', 1, [], [('b2', {W: 'fail'}, None)], 0.0)]
        hil_test._recover_wedged_rows(CFG, CFG['boards'], mret)
        self.assertTrue(seen['skip_flash'])
        self.assertEqual(mret[0][3][0][1], {W: hil_report.WEDGED_RECOVERED, 'device/usbtest': '❌ 10/30'})
        self.assertEqual(mret[0][3][1][1], {W: hil_report.WEDGED_REFUSED_RECOVERED})
        self.assertEqual(mret[1][3][0][1], {W: 'fail'})
        # a raising helper never costs the report
        hil_recover.recover_wedged = lambda *a, **k: 1 / 0
        self.assertEqual(hil_test._recover_wedged_rows(CFG, CFG['boards'], mret), {})


class AfterPool(unittest.TestCase):
    def test_a_recovery_on_an_abort_path_rewrites_the_abort_report(self):
        import hil_test
        calls = []
        self.addCleanup(setattr, hil_recover, 'recover_wedged', hil_recover.recover_wedged)
        self.addCleanup(setattr, hil_test, '_abort_report', hil_test._abort_report)
        hil_test._abort_report = lambda *a, **k: calls.append((a, k))
        args = (('abandoned: worker pool timed out after 1s', [], [], None, None, True, ''), {'timeout_secs': 1})
        hil_recover.recover_wedged = lambda *a, **k: {'b1': {'recovered': True}}
        hil_test._after_pool(CFG, CFG['boards'], [], args)
        self.assertEqual(calls, [args])
        # nothing recovered, or no abort: the report already on disk is the report
        calls.clear()
        hil_recover.recover_wedged = lambda *a, **k: {'b1': {'recovered': False, 'why': 'x'}}
        hil_test._after_pool(CFG, CFG['boards'], [], args)
        hil_recover.recover_wedged = lambda *a, **k: {'b1': {'recovered': True}}
        hil_test._after_pool(CFG, CFG['boards'], [], None)
        self.assertEqual(calls, [])


class Accumulated(unittest.TestCase):
    def test_a_recovered_refusal_recovers_the_earlier_attempts_wedge_cells_too(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        W = hil_report.WEDGED_CELL
        # attempt 1: the board wedged, its variant rows carry the cell and a real failure
        prior = [('b', 1, [], [('b-fs', {W: 'fail', 'device/usbtest': '❌ 10/30'}, None),
                               ('b-hs', {W: 'fail'}, None)], 0.0)]
        # ...beside an unrelated board, itself wedged (a `b-` prefixed name would fall under
        # summarize's documented prefix tolerance, which is not what is under test here)
        prior.append(('zz', 1, [], [('zz', {W: 'fail'}, None)], 0.0))
        hil_report.accumulate_report(prior, rd, True)
        # attempt 2: refused at admission, then recovered post-run
        owned = {'b': ['b-fs', 'b-hs'], 'zz': []}
        hil_report.accumulate_report([('b', 1, [], [('b', {W: hil_report.WEDGED_REFUSED_RECOVERED}, None)], 0.0)],
                                     rd, False, owned=owned)
        report = json.loads((rd / hil_report.REPORT_JSON).read_text())
        cfg = {'boards': [{'name': 'b', 'variant': [{'name': 'b-fs'}, {'name': 'b-hs'}]}, {'name': 'zz'}]}
        row, other = hil_report.summarize(cfg, ['b', 'zz'], report)['results']
        self.assertEqual((row['ran'], row['pass'], row['wedged']), (False, False, False), row)
        cells = {r['board']: r['cells'] for r in report['rows']}
        self.assertEqual(cells['b-fs'][W], hil_report.WEDGED_RECOVERED)
        self.assertEqual(cells['b-hs'][W], hil_report.WEDGED_RECOVERED)
        self.assertEqual(cells['b-fs']['device/usbtest'], '❌ 10/30')
        self.assertEqual(cells['zz'][W], 'fail')                       # not ours: still wedged
        self.assertTrue(other['wedged'])
        # without ownership only the board's own row is rewritten
        rd2 = Path(td.name) / 'two'
        hil_report.accumulate_report(prior, rd2, True)
        hil_report.accumulate_report([('b', 1, [], [('b', {W: hil_report.WEDGED_REFUSED_RECOVERED}, None)], 0.0)], rd2, False)
        cells = {r['board']: r['cells'] for r in json.loads((rd2 / hil_report.REPORT_JSON).read_text())['rows']}
        self.assertEqual(cells['b-fs'][W], 'fail')


class ReportCell(unittest.TestCase):
    def test_a_recovered_refusal_is_not_wedged_and_did_not_run(self):
        cfg = {'boards': [{'name': 'b1'}]}
        report = {'rows': [{'board': 'b1', 'cells': {hil_report.WEDGED_CELL: hil_report.WEDGED_REFUSED_RECOVERED}}]}
        row, = hil_report.summarize(cfg, ['b1'], report)['results']
        self.assertEqual((row['ran'], row['pass'], row['wedged']), (False, False, False))
        self.assertIn('recovered post-run', row['detail'])

    def test_a_recovered_wedge_is_not_wedged_and_still_not_a_pass(self):
        cfg = {'boards': [{'name': 'b1'}]}
        report = {'rows': [{'board': 'b1', 'cells': {'device/usbtest': f'{hil_report.REPORT_CELL["fail"]} 10/30 wedged',
                                                      hil_report.WEDGED_CELL: hil_report.WEDGED_RECOVERED}}]}
        row, = hil_report.summarize(cfg, ['b1'], report)['results']
        self.assertEqual((row['ran'], row['pass'], row['wedged'], row['locked']), (True, False, False, False))
        self.assertIn('wedge recovered post-run (marker cleared)', row['detail'])
        report['rows'][0]['cells'][hil_report.WEDGED_CELL] = 'fail'
        row, = hil_report.summarize(cfg, ['b1'], report)['results']
        self.assertTrue(row['wedged'])
        self.assertNotIn('recovered', row['detail'])


if __name__ == '__main__':
    unittest.main()
