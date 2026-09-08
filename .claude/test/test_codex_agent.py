import io
import json
import subprocess
import sys
from unittest import mock
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LAUNCHER = ROOT / '.claude' / 'codex-agent.py'

sys.path.insert(0, str(LAUNCHER.parent))
codex_agent = __import__('codex-agent')


def run(role, prompt_file=None, schema_file=None):
    """Invoke the launcher. Given paths are used as-is; otherwise real temp
    files are created, since some paths must exist for the run to get that far."""
    with tempfile.TemporaryDirectory() as tmp:
        if prompt_file is None:
            prompt_file = Path(tmp) / 'prompt.txt'
            prompt_file.write_text('review the diff')
        if schema_file is None:
            schema_file = Path(tmp) / 'schema.json'
            schema_file.write_text('{"type": "object"}')
        return subprocess.run(
            [sys.executable, str(LAUNCHER), '--role', role,
             '--prompt-file', str(prompt_file), '--schema-file', str(schema_file)],
            capture_output=True, text=True, cwd=str(ROOT))


class AllowlistTest(unittest.TestCase):
    def test_read_only_roles_is_exactly_two(self):
        self.assertEqual(codex_agent.READ_ONLY_ROLES, frozenset({'code-verifier'}))

    def test_write_role_is_rejected_before_reading_the_adapter(self):
        # code-writer.toml exists on disk, so a rejection here proves the
        # allowlist runs before any adapter path is opened.
        self.assertTrue((ROOT / '.codex' / 'agents' / 'code-writer.toml').is_file())
        result = run('code-writer')
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, '')
        self.assertIn('code-writer', result.stderr)

    def test_unknown_role_is_rejected(self):
        result = run('not-a-role')
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, '')

    def test_resolve_adapter_refuses_a_disallowed_role(self):
        with self.assertRaises(ValueError):
            codex_agent.resolve_adapter(ROOT, 'hil-operator')

    def test_resolve_adapter_reads_an_allowed_role(self):
        adapter = codex_agent.resolve_adapter(ROOT, 'code-verifier')
        self.assertTrue(adapter['model'])
        self.assertTrue(adapter['developer_instructions'].strip())

    def test_a_timeout_is_reported_as_a_timeout(self):
        # `timeout` exits 124 when it kills the child. Reporting that as a bare
        # failure sent a real review's partial stderr up as if the arguments
        # were wrong; the caller must be able to tell the two apart.
        self.assertIn('timed out', codex_agent.failure_detail(124, 'whatever'))
        self.assertNotIn('timed out', codex_agent.failure_detail(1, 'real error'))
        self.assertIn('real error', codex_agent.failure_detail(1, 'real error'))

    def test_missing_prompt_file_exits_one_with_empty_stdout(self):
        result = run('code-verifier', '/nonexistent/p', '/nonexistent/s')
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, '')


class SubprocessTest(unittest.TestCase):
    """The real codex run is expensive, so the failure paths are mocked."""

    def _invoke(self, returncode, stderr='', result_body=None):
        def fake_run(cmd, **kw):
            if result_body is not None:
                # --output-last-message names the file codex is meant to write
                Path(cmd[cmd.index('--output-last-message') + 1]).write_text(result_body)
            return subprocess.CompletedProcess(cmd, returncode, stdout='', stderr=stderr)

        with tempfile.TemporaryDirectory() as tmp:
            prompt = Path(tmp) / 'p.txt'
            schema = Path(tmp) / 's.json'
            prompt.write_text('review')
            schema.write_text('{"type": "object"}')
            argv = ['codex-agent.py', '--role', 'code-verifier',
                    '--prompt-file', str(prompt), '--schema-file', str(schema)]
            with mock.patch.object(sys, 'argv', argv), \
                 mock.patch.object(codex_agent.subprocess, 'run', fake_run), \
                 mock.patch.object(sys, 'stdout', new_callable=io.StringIO) as out:
                code = codex_agent.main()
            return code, out.getvalue()

    def test_success_forwards_the_result_verbatim(self):
        body = json.dumps({'pass': True})
        code, out = self._invoke(0, result_body=body)
        self.assertEqual(code, 0)
        self.assertEqual(out, body)

    def test_nonzero_exit_yields_no_stdout(self):
        code, out = self._invoke(1, stderr='codex blew up', result_body='{"pass": true}')
        self.assertEqual(code, 1)
        self.assertEqual(out, '')

    def test_a_missing_result_file_is_a_failure(self):
        code, out = self._invoke(0, result_body=None)
        self.assertEqual(code, 1)
        self.assertEqual(out, '')

    def test_a_non_json_result_never_reaches_stdout(self):
        with self.assertRaises(json.JSONDecodeError):
            self._invoke(0, result_body='I could not comply.')


if __name__ == '__main__':
    unittest.main()
