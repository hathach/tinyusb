"""Tests for the make-release skill's release.py: bump refuses a malformed or
unchanged version and any file its pattern no longer matches; the PR range
refuses a previous tag that is not an ancestor and any first-parent commit
that is not a merged PR; gh failures surface instead of thinning the list."""
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

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


class PrNumbersTest(unittest.TestCase):
    def test_merge_button_and_squash_subjects_dedupe_and_sort(self):
        self.assertEqual(release.pr_numbers([
            'Merge pull request #3313 from x/y', 'Fix a thing (#3340)', 'Fix a thing (#3313)', '']), [3313, 3340])

    def test_a_direct_push_refuses_the_whole_range(self):
        with self.assertRaises(release.Refused) as cm:
            release.pr_numbers(['Merge pull request #1 from a/b', 'bump version'])
        self.assertIn('bump version', str(cm.exception))


class CliTest(unittest.TestCase):
    """git and gh are stubs on PATH: git answers the ancestor check and the log, gh answers per PR."""

    def run_cli(self, args, ancestor=True, gh_fail=()):
        with tempfile.TemporaryDirectory() as d:
            bin_dir = Path(d) / 'bin'
            bin_dir.mkdir()
            (bin_dir / 'git').write_text(f'''#!/bin/sh
case "$*" in
  *merge-base*) exit {0 if ancestor else 1} ;;
  *log*) printf 'Merge pull request #10 from a/b\\nSquashed (#11)\\n' ;;
esac
''')
            (bin_dir / 'gh').write_text(f'''#!/bin/sh
n=$3
case " {' '.join(map(str, gh_fail))} " in *" $n "*) echo "boom $n" >&2; exit 1 ;; esac
author=hathach; [ "$n" = 11 ] && author='dependabot[bot]'
printf '{{"number": %s, "title": "PR %s", "labels": [{{"name": "Port"}}], "author": {{"login": "%s"}}}}' "$n" "$n" "$author"
''')
            for f in (bin_dir / 'git', bin_dir / 'gh'):
                f.chmod(0o755)
            env = dict(os.environ, PATH=f'{bin_dir}:{os.environ["PATH"]}')
            return subprocess.run([sys.executable, str(SCRIPT), '--root', d, *args],
                                  capture_output=True, text=True, env=env)

    def test_prs_lists_number_title_and_labels(self):
        r = self.run_cli(['prs', '--prev', '0.20.0'])
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout, '#10\tPR 10\t[Port]\n#11\tPR 11\t[Port]\n')

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


if __name__ == '__main__':
    unittest.main()
