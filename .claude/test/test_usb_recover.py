"""usb_recover.sh shield/unshield/shield-status against a fake sysfs tree: modes are
recorded before any chmod and restored from the record only onto the same inodes; an
overlapping shield is refused; a failed acquisition rolls back and keeps no record; a live
foreign owner blocks unshield while a dead one does not. Never run as root: the seams
(USB_RECOVER_SYSFS/USB_RECOVER_STATE) are ignored under root and the partial-acquisition
case chmods a root-owned procfs file expecting EPERM."""
import os
import signal
import subprocess
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / 'skills' / 'usb-kernel-recover' / 'scripts' / 'usb_recover.sh'
ATTRS = ['bNumInterfaces', 'bmAttributes', 'bMaxPower', 'configuration', 'bConfigurationValue',
         'product', 'manufacturer', 'serial', 'avoid_reset_quirk']
MODE = {'bConfigurationValue': 0o644, 'avoid_reset_quirk': 0o644}   # the rest are 0o444, as on ci.lan


def mode(path):
    return os.stat(path).st_mode & 0o777


@unittest.skipIf(os.geteuid() == 0, 'the fake-tree seams are ignored under root')
class Shield(unittest.TestCase):
    def setUp(self):
        self.td = tempfile.TemporaryDirectory()
        self.addCleanup(self.td.cleanup)
        self.sys = Path(self.td.name) / 'sys'
        self.state = Path(self.td.name) / 'state'
        self.dev = self.sys / 'bus/usb/devices'
        self.dev.mkdir(parents=True)
        self.add('usb13')
        self.add('13-1', skip=('manufacturer', 'serial'))   # hubs carry no strings, like the rig's
        self.add('13-1.6')
        self.add('13-1.7')
        self.owner = subprocess.Popen(['sleep', '300'])
        self.addCleanup(self.owner.wait)
        self.addCleanup(self.owner.kill)

    def add(self, name, skip=()):
        d = self.dev / name
        d.mkdir()
        for a in ATTRS:
            if a in skip:
                continue
            (d / a).write_text(f'{name} {a}\n')
            os.chmod(d / a, MODE.get(a, 0o444))

    def run_script(self, *args, ok=True, env=None):
        env = dict(os.environ, USB_RECOVER_SYSFS=str(self.sys), USB_RECOVER_STATE=str(self.state), **(env or {}))
        r = subprocess.run(['bash', str(SCRIPT), *args], env=env, capture_output=True, text=True)
        if ok:
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        else:
            self.assertNotEqual(r.returncode, 0, r.stdout + r.stderr)
        return r.stdout + r.stderr

    def modes(self, *names):
        return {f'{n}/{a}': mode(self.dev / n / a) for n in names for a in ATTRS if (self.dev / n / a).exists()}

    def test_shield_records_then_zeroes_leaf_parent_and_root_and_unshield_restores(self):
        before = self.modes('usb13', '13-1', '13-1.6', '13-1.7')
        out = self.run_script('shield', '13-1.6', str(self.owner.pid))
        self.assertIn('shielded 13-1.6: 25 attribute(s) on 13-1.6,13-1,usb13', out)
        rec = (self.state / '13-1.6').read_text()
        self.assertIn(f'owner {self.owner.pid} ', rec)
        self.assertIn('attr 13-1/bConfigurationValue ', rec)
        self.assertIn('absent 13-1/serial', rec)
        after = self.modes('usb13', '13-1', '13-1.6', '13-1.7')
        for k, m in after.items():
            self.assertEqual(m, before[k] if k.startswith('13-1.7/') else 0, k)   # the sibling is untouched
        # a recorded 644 comes back as 644, which copying from a 444 sibling attribute never could
        out = self.run_script('unshield', '13-1.6', str(self.owner.pid))
        self.assertIn('restored 25 attribute(s), 0 gone', out)
        self.assertEqual(self.modes('usb13', '13-1', '13-1.6', '13-1.7'), before)
        self.assertFalse((self.state / '13-1.6').exists())

    def test_a_shield_on_a_root_port_device_covers_leaf_and_root_only(self):
        out = self.run_script('shield', '13-1', str(self.owner.pid))
        self.assertIn('on 13-1,usb13 ', out)
        self.assertEqual(mode(self.dev / '13-1.6/serial'), 0o444)

    def test_overlapping_shields_are_refused_and_touch_nothing(self):
        self.run_script('shield', '13-1.6', str(self.owner.pid))
        before = self.modes('13-1.7')
        out = self.run_script('shield', '13-1.7', str(self.owner.pid), ok=False)
        self.assertIn('already covered by the shield on 13-1.6', out)
        self.assertEqual(self.modes('13-1.7'), before)
        self.assertFalse((self.state / '13-1.7').exists())

    def test_an_attribute_that_is_not_a_regular_file_stops_the_shield_before_any_change(self):
        (self.dev / 'usb13/product').unlink()
        os.symlink('/proc/version', self.dev / 'usb13/product')
        before = self.modes('13-1', '13-1.6')
        out = self.run_script('shield', '13-1.6', str(self.owner.pid), ok=False)
        self.assertIn('cannot inspect usb13/product (not a regular file); nothing changed', out)
        self.assertEqual(self.modes('13-1', '13-1.6'), before)
        self.assertEqual([p.name for p in self.state.glob('*')], ['.lock'])

    def test_a_failed_acquisition_rolls_back_and_keeps_no_record(self):
        # the root hub is a REAL one: its attributes stat fine but refuse chmod to a
        # non-root caller (EPERM), after the leaf and parent were already zeroed
        real = Path('/sys/bus/usb/devices/usb1')
        if not (real / 'serial').exists():
            self.skipTest('no real root hub to borrow')
        import shutil
        shutil.rmtree(self.dev / 'usb13')
        os.symlink(real, self.dev / 'usb13')
        real_modes = {a: mode(real / a) for a in ATTRS if (real / a).exists()}
        before = self.modes('13-1', '13-1.6')
        out = self.run_script('shield', '13-1.6', str(self.owner.pid), ok=False)
        self.assertIn('chmod 000 usb13/', out)
        self.assertIn('rolled back 16 attribute(s), no record kept', out)
        self.assertEqual(self.modes('13-1', '13-1.6'), before)
        self.assertEqual({a: mode(real / a) for a in real_modes}, real_modes)
        self.assertEqual([p.name for p in self.state.glob('*')], ['.lock'])

    def test_concurrent_shields_on_a_shared_hub_admit_exactly_one(self):
        env = dict(os.environ, USB_RECOVER_SYSFS=str(self.sys), USB_RECOVER_STATE=str(self.state))
        procs = [subprocess.Popen(['bash', str(SCRIPT), 'shield', bp, str(self.owner.pid)], env=env,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                 for bp in ('13-1.6', '13-1.7')]
        outs = [(p.wait(), p.communicate()[0]) for p in procs]
        codes = sorted(rc for rc, _ in outs)
        self.assertEqual(codes, [0, 1], outs)
        loser = next(o for rc, o in outs if rc)
        self.assertIn('already covered by the shield on', loser)
        winner, = [bp for bp in ('13-1.6', '13-1.7') if (self.state / bp).exists()]
        other = '13-1.7' if winner == '13-1.6' else '13-1.6'
        self.assertEqual(mode(self.dev / other / 'serial'), 0o444)
        self.assertEqual(mode(self.dev / 'usb13/serial'), 0)

    def test_an_attribute_that_cannot_be_inspected_keeps_the_record(self):
        self.run_script('shield', '13-1.6', str(self.owner.pid))
        os.chmod(self.dev / 'usb13', 0)           # EACCES on every root-hub attribute, not ENOENT
        self.addCleanup(os.chmod, self.dev / 'usb13', 0o755)
        out = self.run_script('unshield', '13-1.6', str(self.owner.pid), ok=False)
        self.assertIn('could not restore usb13/', out)
        self.assertIn('record', out)
        self.assertIn('kept (restored 16, gone 0)', out)
        self.assertTrue((self.state / '13-1.6').exists())
        self.assertIn('9 uninspectable', self.run_script('shield-status'))
        os.chmod(self.dev / 'usb13', 0o755)
        out = self.run_script('unshield', '13-1.6', str(self.owner.pid))
        self.assertIn('restored 25 attribute(s)', out)     # the 16 again (same mode) plus the 9
        self.assertEqual(mode(self.dev / 'usb13/bConfigurationValue'), 0o644)

    def test_unshield_skips_objects_that_re_enumerated_or_vanished(self):
        self.run_script('shield', '13-1.6', str(self.owner.pid))
        # the leaf re-enumerated: same path, new inodes, kernel-fresh modes
        for a in ATTRS:
            (self.dev / '13-1.6' / a).unlink()
        (self.dev / '13-1.6').rmdir()
        self.add('13-1.6')
        # a fake extra attr file shows nothing else is touched; the parent hub vanished entirely
        for a in ATTRS:
            p = self.dev / '13-1' / a
            if p.exists():
                p.unlink()
        (self.dev / '13-1').rmdir()
        out = self.run_script('unshield', '13-1.6', str(self.owner.pid))
        self.assertIn('restored 9 attribute(s), 16 gone', out)
        self.assertEqual(mode(self.dev / 'usb13/bConfigurationValue'), 0o644)
        self.assertEqual(mode(self.dev / '13-1.6/serial'), 0o444)      # the new object's own mode, untouched
        self.assertFalse((self.state / '13-1.6').exists())

    def test_a_live_foreign_owner_blocks_unshield_and_a_dead_one_does_not(self):
        self.run_script('shield', '13-1.6', str(self.owner.pid))
        out = self.run_script('unshield', '13-1.6', str(os.getpid()), ok=False)
        self.assertIn(f'shielded by live pid {self.owner.pid}', out)
        out = self.run_script('unshield', '13-1.6', ok=False)
        self.assertIn('live pid', out)
        self.assertEqual(mode(self.dev / 'usb13/serial'), 0)
        self.assertIn(f'owner pid {self.owner.pid} alive; 25 attribute(s) still shielded', self.run_script('shield-status'))
        self.owner.send_signal(signal.SIGKILL)
        self.owner.wait()
        self.assertIn(f'owner pid {self.owner.pid} dead', self.run_script('shield-status', '13-1.6'))
        out = self.run_script('unshield', '13-1.6')       # stale record: no pid needed
        self.assertIn('restored 25', out)
        self.assertIn('no shields recorded', self.run_script('shield-status'))

    def test_a_reused_pid_is_not_the_owner(self):
        self.run_script('shield', '13-1.6', str(self.owner.pid))
        rec = self.state / '13-1.6'
        lines = rec.read_text().splitlines()
        pid, start, boot = lines[0].split()[1:]
        lines[0] = f'owner {pid} {int(start) - 1} {boot}'     # same pid, different start time
        rec.write_text('\n'.join(lines) + '\n')
        self.assertIn('dead', self.run_script('shield-status', '13-1.6'))

    def test_a_failed_rollback_keeps_the_record_for_a_later_unshield(self):
        real = Path('/sys/bus/usb/devices/usb1')
        if not (real / 'serial').exists():
            self.skipTest('no real root hub to borrow')
        import shutil
        shutil.rmtree(self.dev / 'usb13')
        os.symlink(real, self.dev / 'usb13')            # acquisition fails at the root hub (EPERM)...
        env = {'USB_RECOVER_TEST_FAIL_CHMOD': str(self.dev / '13-1/bMaxPower')}   # ...and one rollback chmod fails
        out = self.run_script('shield', '13-1.6', str(self.owner.pid), ok=False, env=env)
        self.assertIn('rollback incomplete: 13-1/bMaxPower (chmod failed: injected)', out)
        self.assertIn('KEPT for unshield', out)
        self.assertTrue((self.state / '13-1.6').exists())
        self.assertEqual(mode(self.dev / '13-1/bMaxPower'), 0)
        self.assertIn('1 attribute(s) still shielded, 24 recorded but not shielded', self.run_script('shield-status'))
        out = self.run_script('unshield', '13-1.6', str(self.owner.pid))
        self.assertIn('restored 25 attribute(s), 0 gone', out)
        self.assertEqual(mode(self.dev / '13-1/bMaxPower'), 0o444)
        self.assertFalse((self.state / '13-1.6').exists())

    def test_the_other_actions_still_dispatch(self):
        out = self.run_script('resolve', '/dev/null', ok=False)
        self.assertIn('could not find parent USB device', out)
        self.assertNotIn('command not found', out)

    def test_arguments_are_checked(self):
        self.assertIn('bad usb path', self.run_script('shield', '../etc', str(self.owner.pid), ok=False))
        self.assertIn('is not a pid', self.run_script('shield', '13-1.6', ok=False))
        self.assertIn('not a running process', self.run_script('shield', '13-1.6', '4194300', ok=False))
        self.assertIn('no such usb object', self.run_script('shield', '13-2', str(self.owner.pid), ok=False))
        self.assertIn('no shield record', self.run_script('unshield', '13-1.6', ok=False))


if __name__ == '__main__':
    unittest.main()
