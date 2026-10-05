#!/usr/bin/env python3
"""Tests for .github/actions/setup_toolchain: the toolchain manifest and fetch.sh, with curl stubbed."""
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import unittest

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
ACTION = os.path.join(REPO, '.github', 'actions', 'setup_toolchain')
FETCH = os.path.join(ACTION, 'download', 'fetch.sh')
PAYLOAD = 'toolchain bytes\n'
DIGEST = hashlib.sha256(PAYLOAD.encode()).hexdigest()

# writes CURL_PAYLOAD to the -o file and logs the call, or fails like curl -f on a 404 when CURL_FAIL is set
CURL_STUB = '''#!/bin/sh
echo "$@" >> "$CURL_LOG"
[ -n "$CURL_FAIL" ] && exit 22
while [ $# -gt 0 ]; do
  [ "$1" = -o ] && { printf '%s' "$CURL_PAYLOAD" > "$2"; exit 0; }
  shift
done
exit 2
'''

class Manifest(unittest.TestCase):
    def test_every_entry_pins_an_https_url_and_sha256(self):
        with open(os.path.join(ACTION, 'toolchain.json')) as f:
            manifest = json.load(f)
        for name, entry in manifest.items():
            with self.subTest(name):
                self.assertEqual(set(entry), {'url', 'sha256'})
                self.assertTrue(entry['url'].startswith('https://'))
                self.assertRegex(entry['sha256'], r'^[0-9a-f]{64}$')


class Fetch(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp)
        # fetch.sh reads ../toolchain.json beside itself, so run a copy next to a fixture manifest
        os.mkdir(os.path.join(self.tmp, 'download'))
        self.fetch_sh = shutil.copy(FETCH, os.path.join(self.tmp, 'download', 'fetch.sh'))
        self.bin = os.path.join(self.tmp, 'bin')
        os.mkdir(self.bin)
        self._write_exe('curl', CURL_STUB)
        self.curl_log = os.path.join(self.tmp, 'curl.log')
        self.out = os.path.join(self.tmp, 'toolchain.tar.xz')

    def _write_exe(self, name, text):
        path = os.path.join(self.bin, name)
        with open(path, 'w') as f:
            f.write(text)
        os.chmod(path, 0o755)

    def _manifest(self, **entries):
        with open(os.path.join(self.tmp, 'toolchain.json'), 'w') as f:
            json.dump(entries, f)

    def fetch(self, name='tc', path=None, **env):
        env = {'PATH': path or f'{self.bin}:{os.environ["PATH"]}', 'CURL_LOG': self.curl_log, 'CURL_PAYLOAD': PAYLOAD, **env}
        return subprocess.run([self.fetch_sh, name, self.out], env=env, capture_output=True, text=True)

    def curl_called(self):
        return os.path.exists(self.curl_log)

    def test_matching_digest_succeeds(self):
        self._manifest(tc={'url': 'https://example.invalid/tc.tar.xz', 'sha256': DIGEST})
        r = self.fetch()
        self.assertEqual(r.returncode, 0, r.stderr)
        with open(self.out) as f:
            self.assertEqual(f.read(), PAYLOAD)
        with open(self.curl_log) as f:
            self.assertIn('https://example.invalid/tc.tar.xz', f.read())

    def test_mismatched_digest_fails(self):
        self._manifest(tc={'url': 'https://example.invalid/tc.tar.xz', 'sha256': '0' * 64})
        self.assertNotEqual(self.fetch().returncode, 0)

    def test_unknown_name_fails_before_download(self):
        self._manifest(tc={'url': 'https://example.invalid/tc.tar.xz', 'sha256': DIGEST})
        self.assertNotEqual(self.fetch('other').returncode, 0)
        self.assertFalse(self.curl_called())

    def test_missing_digest_fails_before_download(self):
        self._manifest(tc={'url': 'https://example.invalid/tc.tar.xz'})
        self.assertNotEqual(self.fetch().returncode, 0)
        self.assertFalse(self.curl_called())

    def test_download_failure_stops_before_verification(self):
        self._manifest(tc={'url': 'https://example.invalid/tc.tar.xz', 'sha256': DIGEST})
        self.assertEqual(self.fetch(CURL_FAIL='1').returncode, 22)

    @unittest.skipUnless(shutil.which('shasum') and shutil.which('perl'), 'shasum not installed')
    def test_falls_back_to_shasum_without_sha256sum(self):
        # PATH holds only what fetch.sh and shasum need, so sha256sum is absent as on macOS
        for tool in ('bash', 'dirname', 'jq', 'perl', 'shasum'):
            os.symlink(shutil.which(tool), os.path.join(self.bin, tool))
        self._manifest(good={'url': 'https://example.invalid/a', 'sha256': DIGEST},
                       bad={'url': 'https://example.invalid/b', 'sha256': '0' * 64})
        good = self.fetch('good', path=self.bin)
        self.assertEqual(good.returncode, 0, good.stderr)
        self.assertNotEqual(self.fetch('bad', path=self.bin).returncode, 0)

    def test_native_windows_jq_line_endings(self):
        # native jq.exe turns every LF it prints into CRLF; command substitution keeps the CR
        self._write_exe('jq', f'#!/bin/bash\nset -o pipefail\n{shutil.which("jq")} "$@" | perl -pe "s/\\n/\\r\\n/"\n')
        self._manifest(tc={'url': 'https://example.invalid/tc.tar.xz', 'sha256': DIGEST})
        r = self.fetch()
        self.assertEqual(r.returncode, 0, r.stderr)
        with open(self.curl_log) as f:
            self.assertIn('https://example.invalid/tc.tar.xz -o', f.read())

    def test_curl_restricts_to_https_including_redirects(self):
        # a behavioural check needs a TLS server; pin the flags that enforce it instead
        with open(FETCH) as f:
            script = f.read()
        self.assertIn("--proto '=https'", script)
        self.assertIn("--proto-redir '=https'", script)


if __name__ == '__main__':
    unittest.main()
