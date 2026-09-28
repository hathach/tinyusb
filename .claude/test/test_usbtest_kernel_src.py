"""kernel_src.py against a local HTTP server standing in for git.kernel.org and a trimmed
usbtest.c fixture. Plumbing only: the real fetch is proved by a run against kernel.org."""
import http.server
import importlib.util
import os
import subprocess
import sys
import tempfile
import threading
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SCRIPT = REPO / '.claude' / 'skills' / 'usbtest' / 'scripts' / 'kernel_src.py'
spec = importlib.util.spec_from_file_location('kernel_src', SCRIPT)
kernel_src = importlib.util.module_from_spec(spec)
spec.loader.exec_module(kernel_src)

# The shapes the parser relies on, as usbtest.c writes them: a split-style header, a nested
# switch whose labels are indented deeper, fall-through labels, and each wait API.
USBTEST_C = '''\
static int simple_io(struct usbtest_dev *tdev, struct urb *urb)
{
\tif (!wait_for_completion_timeout(&completion, expire)) {
\t}
}

static int
test_queue(struct usbtest_dev *dev, struct usbtest_param_32 *param)
{
\t/* wait_for_completion(&commented) is not a call site */
\twait_for_completion(&context.done);
}

static int ch9_postconfig(struct usbtest_dev *dev)
{
\tswitch (i) {
\t\tcase 5:\t\t/* nested label, not a test case */
\t\t\tbreak;
\t}
}

static long
usbtest_do_ioctl(struct usb_interface *intf, struct usbtest_param_32 *param)
{
\tint\tretval = -EOPNOTSUPP;

\tswitch (param->test_num) {

\tcase 0:
\t\tretval = 0;
\t\tbreak;

\tcase 5:
\t\tswitch (x) {
\t\tcase 7:
\t\t\tbreak;
\t\t}
\t\tretval = perform(dev);
\t\tbreak;
\tcase 15:
\t/* shared with 16 */
\tcase 16:
\t\tretval = test_queue(dev, param);
\t\tbreak;
\t}
\treturn retval;
}
'''
TESTUSB_C = 'int main(void) { return 0; }\n'


class Server:
    """Serves <root>/<path> for GET <path>?h=<tag>, counting requests."""
    def __init__(self, root):
        self.hits = []
        hits = self.hits

        class Handler(http.server.SimpleHTTPRequestHandler):
            def __init__(self, *a, **kw):
                super().__init__(*a, directory=str(root), **kw)

            def do_GET(self):
                hits.append(self.path)
                super().do_GET()

            def log_message(self, *a):
                pass

        self.httpd = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        self.url = f'http://127.0.0.1:{self.httpd.server_address[1]}'
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()

    def close(self):
        self.httpd.shutdown()
        self.httpd.server_close()


class ReleaseToTag(unittest.TestCase):
    def test_upstream_forms(self):
        for release, tag in [('6.12.107', 'v6.12.107'), ('6.13.0', 'v6.13'),
                             ('6.12.0-rc3', 'v6.12-rc3')]:
            self.assertEqual(kernel_src.tag_for_release(release), (tag, 'upstream'), release)

    def test_debian_13_is_only_a_candidate(self):
        for release in ('6.12.107+deb13-amd64', '6.12.107+deb13-cloud-amd64', '6.12.107+deb14-arm64'):
            tag, note = kernel_src.tag_for_release(release)
            self.assertEqual(tag, 'v6.12.107')
            self.assertIn('upstream candidate', note)

    def test_refused_forms(self):
        for release in ('6.1.0-18-amd64',             # Debian <= 12 ABI name
                        '6.1.123+deb12-amd64',        # Debian 12 with the new suffix
                        '6.8.0-45-generic',           # Ubuntu ABI name
                        '6.14.0-0.rc3.29.fc42.x86_64',  # Fedora rc
                        '6.12.0-061200rc3-generic',   # Ubuntu mainline rc
                        '6.12.48-rc1',                # no rc of a stable point release
                        '6.12.48-arch1-1', '6.12', ''):
            self.assertIsNone(kernel_src.tag_for_release(release), release)


class CaseBlock(unittest.TestCase):
    lines = USBTEST_C.splitlines()

    def numbers(self, num):
        return [text.strip() for _, text in kernel_src.case_block(self.lines, num)]

    def test_block_runs_to_the_next_top_level_label(self):
        block = self.numbers(5)
        self.assertEqual(block[0], 'case 5:')
        self.assertIn('case 7:', block)            # nested label stays inside the block
        self.assertEqual(block[-1], 'break;')
        self.assertNotIn('case 15:', block)

    def test_fall_through_label_carries_the_shared_body(self):
        shared = ['retval = test_queue(dev, param);', 'break;']
        self.assertEqual(self.numbers(15), ['case 15:', '/* shared with 16 */', 'case 16:', *shared])
        self.assertEqual(self.numbers(16), ['case 16:', *shared])   # last case: up to the switch end

    def test_line_numbers_are_one_based(self):
        n, text = kernel_src.case_block(self.lines, 0)[0]
        self.assertEqual(self.lines[n - 1], text)

    def test_missing_nested_or_duplicate_label_fails(self):
        with self.assertRaises(SystemExit) as e:
            kernel_src.case_block(self.lines, 7)       # exists only nested
        self.assertEqual(e.exception.code, 1)
        dup = USBTEST_C.replace('\tcase 0:', '\tcase 0:\n\tcase 0:')
        with self.assertRaises(SystemExit):
            kernel_src.case_block(dup.splitlines(), 0)
        with self.assertRaises(SystemExit):
            kernel_src.case_block(USBTEST_C.replace('usbtest_do_ioctl', 'other').splitlines(), 0)


class WaitSites(unittest.TestCase):
    def test_each_call_with_its_function_and_api(self):
        sites = kernel_src.wait_sites(USBTEST_C.splitlines())
        self.assertEqual([(api, what, func) for _, api, what, func in sites],
                         [('wait_for_completion_timeout', 'timeout', 'simple_io'),
                          ('wait_for_completion', 'no timeout', 'test_queue')])


class Cli(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())
        self.addCleanup(subprocess.run, ['rm', '-rf', str(self.tmp)])
        src = self.tmp / 'srv'
        for rel, text in (('drivers/usb/misc/usbtest.c', USBTEST_C), ('tools/usb/testusb.c', TESTUSB_C)):
            (src / rel).parent.mkdir(parents=True, exist_ok=True)
            (src / rel).write_text(text)
        self.src = src
        self.server = Server(src)
        self.addCleanup(self.server.close)

    def run_script(self, *args):
        env = dict(os.environ, KERNEL_SRC_BASE_URL=self.server.url, PYTHONDONTWRITEBYTECODE='1')
        return subprocess.run([sys.executable, str(SCRIPT), '--cache', str(self.tmp / 'cache'), *args],
                              capture_output=True, text=True, env=env, timeout=60)

    def test_fetch_prints_case_and_waits(self):
        r = self.run_script('--release', '6.12.107+deb13-amd64', '--case', '16')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('tag v6.12.107 (upstream candidate', r.stdout)
        lines = USBTEST_C.splitlines()
        first = lines.index('\tcase 16:') + 1
        block = [f'{n:5d}  {lines[n - 1]}' for n in range(first, first + 3)]
        waits = ['    3  simple_io: wait_for_completion_timeout (timeout)',
                 '   11  test_queue: wait_for_completion (no timeout)']
        self.assertEqual(r.stdout.splitlines()[3:],
                         ['', '== case 16 in usbtest_do_ioctl()', *block, '',
                          '== completion waits in usbtest.c (API only; read what the case calls)', *waits])
        self.assertEqual((self.tmp / 'cache' / 'v6.12.107' / 'usbtest.c').read_text(), USBTEST_C)
        self.assertEqual(sorted(self.server.hits),
                         ['/drivers/usb/misc/usbtest.c?h=v6.12.107', '/tools/usb/testusb.c?h=v6.12.107'])

    def test_cache_is_reused(self):
        self.assertEqual(self.run_script('--tag', 'v6.12.107').returncode, 0)
        self.assertEqual(self.run_script('--tag', 'v6.12.107').returncode, 0)
        self.assertEqual(len(self.server.hits), 2)

    def test_fetch_failures_exit_1_and_leave_no_file(self):
        (self.src / 'tools/usb/testusb.c').unlink()
        r = self.run_script('--tag', 'v6.12.107')
        self.assertEqual(r.returncode, 1)
        self.assertIn('cannot fetch tools/usb/testusb.c', r.stderr)
        self.assertFalse((self.tmp / 'cache' / 'v6.12.107' / 'testusb.c').exists())
        (self.src / 'tools/usb/testusb.c').write_text('')
        r = self.run_script('--tag', 'v6.12.107')
        self.assertEqual(r.returncode, 1)
        self.assertIn('came back empty', r.stderr)

    def test_usage_errors_exit_2_before_any_fetch(self):
        for args in (['--release', '6.8.0-45-generic'], ['--tag', '6.12.107'], ['--tag', 'v6/../x'],
                     ['--tag', 'v6.12.107', '--case', '100'], []):
            r = self.run_script(*args)
            self.assertEqual(r.returncode, 2, args)
        self.assertEqual(self.server.hits, [])

    def test_any_tag_shape_is_fetched_and_a_missing_tag_fails_there(self):
        self.server.close()
        self.server = Server(self.tmp / 'empty')        # nothing served: every tag is missing
        self.addCleanup(self.server.close)
        (self.tmp / 'empty').mkdir()
        r = self.run_script('--tag', 'v6.12.0')
        self.assertEqual(r.returncode, 1)
        self.assertEqual(self.server.hits, ['/drivers/usb/misc/usbtest.c?h=v6.12.0'])

    def test_unknown_case_exits_1(self):
        r = self.run_script('--tag', 'v6.12.107', '--case', '7')
        self.assertEqual(r.returncode, 1)
        self.assertIn('case 7: found 0 top-level labels', r.stderr)


if __name__ == '__main__':
    unittest.main()
