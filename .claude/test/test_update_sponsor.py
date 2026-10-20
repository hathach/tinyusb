"""Tests for the update-sponsor skill's update_sponsor.py, with gh and codespell answered
in-process (no real CLI runs): gh's JSON is read as UTF-8 whatever the locale, and the codespell
guard reads the words codespell reports from a probe file it could open."""
import importlib.util
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

SCRIPT = Path(__file__).resolve().parents[1] / 'skills' / 'update-sponsor' / 'update_sponsor.py'
spec = importlib.util.spec_from_file_location('update_sponsor', SCRIPT)
update_sponsor = importlib.util.module_from_spec(spec)
spec.loader.exec_module(update_sponsor)


def completed(argv, out, kw, rc=0):
    """What subprocess.run returns for UTF-8 output `out`, decoded as the caller asked under a
    legacy code page's locale: bytes when it asked for no text."""
    out = out.encode('utf-8')
    if kw.get('encoding') or kw.get('text'):
        out = out.decode(kw.get('encoding') or 'cp1252', kw.get('errors') or 'strict')
    return subprocess.CompletedProcess(argv, rc, out, out[:0])


class GhTest(unittest.TestCase):
    def test_label_names_come_back_as_github_sent_them(self):
        def run(argv, **kw):
            self.assertEqual(argv[0], 'gh')
            return completed(argv, f'{update_sponsor.L_PRIO_TOP}\n', kw)
        with mock.patch.object(update_sponsor.subprocess, 'run', run):
            self.assertEqual(update_sponsor.gh('label', 'list'), f'{update_sponsor.L_PRIO_TOP}\n')


TYPO = 'te' + 'h'   # spelled out so the repository's own codespell hook leaves it alone


class CodespellTest(unittest.TestCase):
    def test_the_words_codespell_flags_in_the_probe(self):
        seen = []

        def run(argv, **kw):
            self.assertEqual(argv[0], 'codespell')
            probe = argv[-1]
            seen.append(probe)
            # codespell opens the probe itself: it must be closed and still there
            self.assertIn(f'+{TYPO} \u00f8sterberg\n', Path(probe).read_text(encoding='utf-8'))
            return completed(argv, f'{probe}:1: {TYPO} ==> the\n', kw, rc=65)
        with mock.patch.object(update_sponsor.subprocess, 'run', run):
            self.assertEqual(update_sponsor.codespell_collisions('a\n', f'{TYPO} \u00f8sterberg\n'), [TYPO])
        self.assertFalse(Path(seen[0]).exists(), 'the probe is removed afterwards')

    def test_a_probe_that_could_not_be_written_is_removed(self):
        made, real = [], tempfile.NamedTemporaryFile

        def full(*a, **kw):
            f = real(*a, **kw)
            made.append(f.name)
            f.write = mock.Mock(side_effect=OSError(28, 'No space left on device'))
            return f
        with mock.patch.object(update_sponsor.tempfile, 'NamedTemporaryFile', full), self.assertRaises(OSError):
            update_sponsor.codespell_collisions('a\n', 'b\n')
        self.assertFalse(Path(made[0]).exists())

    def test_no_codespell_is_a_note_not_a_failure(self):
        def run(argv, **kw):
            raise FileNotFoundError(argv[0])
        with mock.patch.object(update_sponsor.subprocess, 'run', run), \
                mock.patch('builtins.print') as note:
            self.assertEqual(update_sponsor.codespell_collisions('a\n', 'b\n'), [])
        self.assertIn('UNCHECKED', note.call_args[0][0])


if __name__ == '__main__':
    unittest.main()
