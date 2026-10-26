"""Tests for the make-release skill's release.py: bump refuses a malformed or
unchanged version and any file its pattern no longer matches; the PR range
refuses a previous tag that is not an ancestor and any first-parent commit
that is not a merged PR; gh failures surface instead of thinning the list."""
import importlib.util
import io
import json
import os
import subprocess
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest import mock

SCRIPT = Path(__file__).resolve().parents[1] / 'skills' / 'make-release' / 'scripts' / 'release.py'
spec = importlib.util.spec_from_file_location('release', SCRIPT)
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)

OPTION_H = '#define TUSB_VERSION_MAJOR     0\n#define TUSB_VERSION_MINOR     21\n#define TUSB_VERSION_REVISION  0\n'
REPOSITORY_YML = 'repo:\n  versions:\n    "0.21.0": "0.21.0"\n    "0-latest": "0.21.0"\n    "0-dev": "0.0.0"\n'
LIBRARY_JSON = '{\n    "name": "TinyUSB",\n    "version": "0.21.0",\n}\n'
SONAR = 'sonar.projectKey=tinyusb\nsonar.projectVersion=0.21.0\n'


def fixture(root):
    (root / 'src').mkdir()
    (root / 'src/tusb_option.h').write_text(OPTION_H)
    (root / 'repository.yml').write_text(REPOSITORY_YML)
    (root / 'library.json').write_text(LIBRARY_JSON)
    (root / 'sonar-project.properties').write_text(SONAR)


class BumpTest(unittest.TestCase):
    def test_writes_the_version_into_all_four_files(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            fixture(root)
            changed = release.bump(root, '0.22.1')
            self.assertEqual([p.name for p in changed],
                             ['tusb_option.h', 'repository.yml', 'library.json', 'sonar-project.properties'])
            self.assertEqual(release.current_version((root / 'src/tusb_option.h').read_text()), '0.22.1')
            self.assertIn('    "0.22.1": "0.22.1"\n    "0-latest": "0.22.1"\n    "0-dev"', (root / 'repository.yml').read_text())
            self.assertIn('    "version": "0.22.1"', (root / 'library.json').read_text())
            self.assertIn('sonar.projectVersion=0.22.1\n', (root / 'sonar-project.properties').read_text())

    def test_refuses_a_malformed_or_current_version_before_touching_anything(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            fixture(root)
            for bad in ('0.22', 'v0.22.0', '0.21.0'):
                with self.assertRaises(release.Refused):
                    release.bump(root, bad)
            self.assertEqual((root / 'src/tusb_option.h').read_text(), OPTION_H)

    def test_refuses_when_a_file_no_longer_matches_its_pattern(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            fixture(root)
            (root / 'library.json').write_text('{\n  "version": "0.21.0"\n}\n')  # two-space indent
            with self.assertRaises(release.Refused) as cm:
                release.bump(root, '0.22.0')
            self.assertIn('library.json', str(cm.exception))
            self.assertEqual((root / 'src/tusb_option.h').read_text(), OPTION_H)
            self.assertEqual((root / 'repository.yml').read_text(), REPOSITORY_YML)
            self.assertEqual((root / 'sonar-project.properties').read_text(), SONAR)

    def test_a_new_major_gets_its_own_alias_and_the_old_major_keeps_its_last_release(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            fixture(root)
            release.bump(root, '1.0.0')
            text = (root / 'repository.yml').read_text()
            self.assertIn('    "1.0.0": "1.0.0"\n    "1-latest": "1.0.0"\n    "0-latest": "0.21.0"\n', text)
            release.bump(root, '1.0.1')
            text = (root / 'repository.yml').read_text()
            self.assertIn('    "1.0.1": "1.0.1"\n    "1-latest": "1.0.1"\n    "0-latest": "0.21.0"\n', text)
            self.assertEqual(text.count('-latest'), 2)

    def test_repository_yml_already_listing_the_version_gets_no_second_entry_but_a_current_alias(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            fixture(root)
            (root / 'repository.yml').write_text(REPOSITORY_YML.replace('"0.21.0": "0.21.0"', '"0.22.0": "0.22.0"'))
            release.bump(root, '0.22.0')
            text = (root / 'repository.yml').read_text()
            self.assertEqual(text.count('"0.22.0": "0.22.0"'), 1)
            self.assertIn('"0-latest": "0.22.0"', text)

    def test_files_are_read_and_written_as_utf8_with_lf_whatever_the_locale(self):
        # bump runs no git or gh, so the script can run as a process here, outside UTF-8 mode and,
        # on POSIX, in an ASCII locale
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            fixture(root)
            lib = LIBRARY_JSON.replace('"name": "TinyUSB",', '"name": "TinyUSB",\n    "authors": "ø ✔",')
            (root / 'library.json').write_bytes(lib.encode('utf-8'))
            env = {**{k: v for k, v in os.environ.items() if k != 'PYTHONUTF8'}, 'LC_ALL': 'C', 'PYTHONCOERCECLOCALE': '0'}
            r = subprocess.run([sys.executable, '-X', 'utf8=0', str(SCRIPT), '--root', d, 'bump', '0.22.1'],
                               capture_output=True, env=env, timeout=60)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertEqual((root / 'library.json').read_bytes(), lib.replace('0.21.0', '0.22.1').encode('utf-8'))


class PrNumbersTest(unittest.TestCase):
    def test_merge_button_and_squash_subjects_dedupe_and_sort(self):
        self.assertEqual(release.pr_numbers([
            'Merge pull request #3313 from x/y', 'Fix a thing (#3340)', 'Fix a thing (#3313)', '']), [3313, 3340])

    def test_a_direct_push_refuses_the_whole_range(self):
        with self.assertRaises(release.Refused) as cm:
            release.pr_numbers(['Merge pull request #1 from a/b', 'bump version'])
        self.assertIn('bump version', str(cm.exception))


class CliTest(unittest.TestCase):
    """main() in-process with git and gh answered by a stand-in for subprocess.run, so no real
    git or gh can run: git answers the ancestor check and the log, gh answers per PR. Their output
    is UTF-8 bytes, decoded the way subprocess.run would with the caller's arguments under a
    legacy code page's locale, and stdout and stderr are a console in a legacy code page."""

    def run_cli(self, args, ancestor=True, gh_fail=()):
        def run(argv, capture_output=False, text=False, encoding=None, cwd=None):
            out, err, rc = '', '', 0
            if argv[0] == 'git':
                rc = 0 if 'merge-base' not in argv or ancestor else 1
                out = 'Merge pull request #10 from a/b\nSquashed (#11)\n' if 'log' in argv else ''
            elif argv[0] == 'gh':
                n = int(argv[3])
                if n in gh_fail:
                    err, rc = f'boom {n}\n', 1
                else:
                    out = json.dumps({'number': n, 'title': f'PR {n} \u2714', 'labels': [{'name': 'Port \U0001f6a8'}],
                                      'author': {'login': 'dependabot[bot]' if n == 11 else 'hathach'}}, ensure_ascii=False)
            else:
                raise AssertionError(f'unexpected command {argv}')
            out, err = out.encode('utf-8'), err.encode('utf-8')
            if encoding or text:
                out, err = (s.decode(encoding or 'cp1252') for s in (out, err))
            return subprocess.CompletedProcess(argv, rc, out, err)

        # Windows prints UTF-8 whatever the console; elsewhere stdout keeps the encoding and errors it was given
        console = [io.TextIOWrapper(io.BytesIO(), encoding='cp1252' if os.name == 'nt' else 'ascii',
                                    errors='strict' if os.name == 'nt' else 'backslashreplace', newline='\n')
                   for _ in range(2)]
        with tempfile.TemporaryDirectory() as d, mock.patch.object(release.subprocess, 'run', run), \
                mock.patch.object(sys, 'argv', [str(SCRIPT), '--root', d, *args]), \
                mock.patch.object(sys, 'stdout', console[0]), mock.patch.object(sys, 'stderr', console[1]):
            code = release.main()
        for c in console:
            c.flush()
        return types.SimpleNamespace(returncode=code, stdout=console[0].buffer.getvalue().decode('utf-8'),
                                     stderr=console[1].buffer.getvalue().decode('utf-8'))

    def test_prs_lists_number_title_and_labels(self):
        r = self.run_cli(['prs', '--prev', '0.20.0'])
        self.assertEqual(r.returncode, 0, r.stderr)
        title, label = ('PR {} \u2714', 'Port \U0001f6a8') if os.name == 'nt' else ('PR {} \\u2714', 'Port \\U0001f6a8')
        self.assertEqual(r.stdout, f'#10\t{title.format(10)}\t[{label}]\n#11\t{title.format(11)}\t[{label}]\n')

    def test_contributors_drops_bots(self):
        r = self.run_cli(['contributors', '--prev', '0.20.0'])
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout, '@hathach\n')

    def test_prev_not_an_ancestor_is_refused(self):
        r = self.run_cli(['prs', '--prev', '0.20.0'], ancestor=False)
        self.assertEqual(r.returncode, 1)
        self.assertIn('not an ancestor of HEAD', r.stderr)

    def test_a_gh_failure_is_reported_not_skipped(self):
        r = self.run_cli(['prs', '--prev', '0.20.0'], gh_fail=[11])
        self.assertEqual(r.returncode, 1)
        self.assertIn('gh pr view 11 failed 3 times: boom 11', r.stderr)
        self.assertEqual(r.stdout, '')


@unittest.skipIf(os.name == 'nt', 'the stubs are #! scripts Windows cannot exec: a miss would run the real git or gh')
class ProcessTest(unittest.TestCase):
    """The script as a process, git and gh stubs on PATH: main()'s status is the exit status."""

    def run_script(self, args, ancestor=True):
        with tempfile.TemporaryDirectory() as d:
            bin_dir = Path(d) / 'bin'
            bin_dir.mkdir()
            (bin_dir / 'git').write_text(f'''#!/bin/sh
case "$*" in
  *merge-base*) exit {0 if ancestor else 1} ;;
  *log*) printf 'Merge pull request #10 from a/b\\n' ;;
esac
''')
            (bin_dir / 'gh').write_text('''#!/bin/sh
printf '{"number": %s, "title": "PR %s", "labels": [], "author": {"login": "hathach"}}' "$3" "$3"
''')
            for f in (bin_dir / 'git', bin_dir / 'gh'):
                f.chmod(0o755)
            env = dict(os.environ, PATH=f'{bin_dir}{os.pathsep}{os.environ["PATH"]}')
            return subprocess.run([sys.executable, str(SCRIPT), '--root', d, *args],
                                  capture_output=True, text=True, env=env, timeout=60)

    def test_the_exit_status_is_mains(self):
        ok = self.run_script(['prs', '--prev', '0.20.0'])
        self.assertEqual((ok.returncode, ok.stdout), (0, '#10\tPR 10\t[]\n'), ok.stderr)
        refused = self.run_script(['prs', '--prev', '0.20.0'], ancestor=False)
        self.assertEqual(refused.returncode, 1)
        self.assertIn('not an ancestor of HEAD', refused.stderr)


if __name__ == '__main__':
    unittest.main()
