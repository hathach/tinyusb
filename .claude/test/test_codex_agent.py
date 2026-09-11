import io
import json
import os
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

ADAPTER = codex_agent.role(ROOT)
SCHEMA = {'type': 'object', 'required': ['pass']}
EVENTS = '{"type":"thread.started","thread_id":"t-42"}\n{"type":"turn.completed"}\n'


class InputTest(unittest.TestCase):
    def test_prompt_and_schema_are_required(self):
        for bad in ['{}', '{"prompt": "x"}', '{"schema": {}}', '{"prompt": " ", "schema": {}}',
                    '{"prompt": "x", "schema": []}', '[]']:
            with self.assertRaises(ValueError, msg=bad):
                codex_agent.read_input(io.StringIO(bad))

    def test_bad_input_exits_one_with_empty_stdout(self):
        result = subprocess.run([sys.executable, str(LAUNCHER)], input='{"prompt": "x"}',
                                capture_output=True, text=True, cwd='/')
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, '')
        self.assertIn('prompt', result.stderr)


class CommandTest(unittest.TestCase):
    def test_plain_mode_is_a_sandboxed_exec_with_the_schema(self):
        cmd = codex_agent.command(ROOT, ADAPTER, Path('/j'), review=False)
        self.assertEqual(cmd[:6], ['codex', 'exec', '-C', str(ROOT), '--sandbox', 'read-only'])
        self.assertEqual(cmd[cmd.index('--output-schema') + 1], '/j/schema.json')
        self.assertEqual(cmd[cmd.index('-o') + 1], '/j/result.json')
        self.assertEqual(cmd[-2:], ['--json', '-'])

    def test_review_mode_is_read_only_by_config_override(self):
        # `codex exec review` has no --sandbox flag; without the override it
        # inherits the user's config, which may be danger-full-access.
        cmd = codex_agent.command(ROOT, ADAPTER, Path('/j'), review=True)
        self.assertEqual(cmd[:5], ['codex', 'exec', 'review', '-c', 'sandbox_mode=read-only'])
        self.assertNotIn('--sandbox', cmd)
        self.assertNotIn('--output-schema', cmd)  # ignored by the reviewer
        self.assertNotIn('-C', cmd)

    def test_model_and_effort_come_from_the_codex_adapter(self):
        for review in (False, True):
            cmd = codex_agent.command(ROOT, ADAPTER, Path('/j'), review)
            self.assertEqual(cmd[cmd.index('-m') + 1], ADAPTER['model'])
            self.assertIn(f'model_reasoning_effort={ADAPTER["model_reasoning_effort"]}', cmd)

    def test_no_mode_can_widen_the_sandbox(self):
        modes = [dict(review=False), dict(review=True), dict(review=True, resume='t-1')]
        for mode in modes:
            cmd = codex_agent.command(ROOT, ADAPTER, Path('/j'), **mode)
            self.assertTrue('--sandbox' in cmd or '-c' in cmd, cmd)
            if '--sandbox' in cmd:
                self.assertEqual(cmd[cmd.index('--sandbox') + 1], 'read-only')
            else:
                self.assertIn('sandbox_mode=read-only', cmd)
            for word in ('workspace-write', 'danger-full-access', '--add-dir'):
                self.assertNotIn(word, cmd)

    def test_resume_is_the_conversion_turn_on_the_same_thread(self):
        cmd = codex_agent.command(ROOT, ADAPTER, Path('/j'), review=True, resume='t-1')
        self.assertEqual(cmd[:4], ['codex', 'exec', 'resume', 't-1'])
        self.assertEqual(cmd[cmd.index('-o') + 1], '/j/result.json')
        self.assertEqual(cmd[-2:], ['--json', '-'])


class ThreadIdTest(unittest.TestCase):
    def test_first_thread_started_event_wins(self):
        with tempfile.TemporaryDirectory() as tmp:
            f = Path(tmp) / 'events.jsonl'
            f.write_text('garbage\n' + EVENTS + '{"type":"thread.started","thread_id":"later"}\n')
            self.assertEqual(codex_agent.thread_id(f), 't-42')
            f.write_text('{"type":"turn.completed"}\n')
            self.assertIsNone(codex_agent.thread_id(f))


class RunTest(unittest.TestCase):
    """The real codex run is expensive, so subprocess.run is faked per outcome."""

    def _run(self, data, outcome, result_body='{"pass": true}', recovered_body=None):
        seen = {'cmds': []}

        def fake_run(cmd, **kw):
            seen['cmds'].append(cmd)
            if cmd[2] == 'resume':  # the conversion turn
                seen['recover'] = Path(kw['stdin'].name).read_text()
                if outcome == 'recover-timeout':
                    raise subprocess.TimeoutExpired(cmd, kw['timeout'])
                if recovered_body is not None:
                    Path(kw['stdout'].name).with_name('result.json').write_text(recovered_body)
                return subprocess.CompletedProcess(cmd, 0)
            seen['cwd'] = kw['cwd']
            seen['prompt'] = Path(kw['stdin'].name).read_text()
            kw['stdout'].write(EVENTS)
            kw['stderr'].write('rmcp noise\nreal error\n')
            if outcome == 'timeout':
                raise subprocess.TimeoutExpired(cmd, kw['timeout'])
            if outcome == 'ok' and result_body is not None:
                Path(kw['stdout'].name).with_name('result.json').write_text(result_body)
            return subprocess.CompletedProcess(cmd, 0 if outcome in ('ok', 'recover-timeout') else 3)

        with tempfile.TemporaryDirectory() as tmp, \
             mock.patch.object(codex_agent, 'JOBS', Path(tmp)), \
             mock.patch.object(codex_agent.subprocess, 'run', fake_run):
            try:
                out = codex_agent.run(data, ROOT, stamp='s')
            except RuntimeError as e:
                return None, str(e), seen
            seen['files'] = sorted(p.name for p in Path(out['job']).iterdir())
            return out, None, seen

    def test_success_returns_an_envelope_with_provenance(self):
        out, err, seen = self._run({'prompt': 'review it', 'schema': SCHEMA}, 'ok')
        self.assertIsNone(err)
        self.assertEqual(out['status'], 'ok')
        self.assertIsNone(out['error'])
        self.assertEqual(out['result'], {'pass': True})
        self.assertEqual(out['thread'], 't-42')
        self.assertTrue(out['job'].endswith('/s-%d' % os.getpid()))
        self.assertEqual(seen['files'], ['events.jsonl', 'prompt.txt', 'result.json', 'schema.json', 'stderr.txt'])
        self.assertEqual(seen['cwd'], ROOT)

    def test_prompt_carries_the_role_and_only_review_mode_carries_the_schema(self):
        _, _, plain = self._run({'prompt': 'review it', 'schema': SCHEMA}, 'ok')
        _, _, review = self._run({'prompt': 'review it', 'schema': SCHEMA, 'review': True}, 'ok')
        for seen in (plain, review):
            self.assertTrue(seen['prompt'].startswith(ADAPTER['developer_instructions'].rstrip()))
            self.assertIn('review it', seen['prompt'])
        self.assertNotIn(codex_agent.OUTPUT_CONTRACT, plain['prompt'])
        self.assertIn(codex_agent.OUTPUT_CONTRACT + json.dumps(SCHEMA), review['prompt'])
        self.assertEqual(plain['cmds'][0][1:3], ['exec', '-C'])
        self.assertEqual(review['cmds'][0][1:3], ['exec', 'review'])

    def test_a_timeout_in_the_conversion_turn_is_the_same_envelope(self):
        out, err, seen = self._run({'prompt': 'x', 'schema': SCHEMA, 'review': True}, 'recover-timeout',
                                   result_body=None)
        self.assertIsNone(err)
        self.assertIn('recover', seen)
        self.assertEqual(out['status'], 'timeout')
        self.assertIsNone(out['result'])
        self.assertIn('timed out', out['error'])

    def test_timeout_is_an_envelope_without_a_result(self):
        out, err, seen = self._run({'prompt': 'x', 'schema': SCHEMA}, 'timeout')
        self.assertIsNone(err)
        self.assertEqual(out['status'], 'timeout')
        self.assertIsNone(out['result'])
        self.assertIn('timed out', out['error'])
        self.assertEqual(out['thread'], 't-42')
        self.assertIn('events.jsonl', seen['files'])

    def test_nonzero_exit_reports_stderr_without_mcp_noise(self):
        out, err, _ = self._run({'prompt': 'x', 'schema': SCHEMA}, 'fail')
        self.assertIsNone(out)
        self.assertIn('exited 3', err)
        self.assertIn('real error', err)
        self.assertNotIn('rmcp', err)

    def test_missing_or_non_json_result_is_a_failure_in_plain_mode(self):
        for body in (None, 'I could not comply.'):
            out, err, seen = self._run({'prompt': 'x', 'schema': SCHEMA}, 'ok', result_body=body)
            self.assertIsNone(out, body)
            self.assertIn('no JSON result', err)
            self.assertEqual(len(seen['cmds']), 1)  # plain mode never resumes

    def test_review_prose_is_converted_by_one_resume_turn(self):
        # The reviewer ignores --output-schema and sometimes answers in its own
        # prose format; the conversion turn must restate the caller's rules
        # (severity format lives there) and the schema, on the same thread.
        out, err, seen = self._run({'prompt': 'rules here', 'schema': SCHEMA, 'review': True}, 'ok',
                                   result_body='- [P1] prose finding', recovered_body='{"pass": false}')
        self.assertIsNone(err)
        self.assertEqual(out['result'], {'pass': False})
        self.assertEqual(out['thread'], 't-42')
        self.assertEqual([c[1:3] for c in seen['cmds']], [['exec', 'review'], ['exec', 'resume']])
        resume = seen['cmds'][1]
        self.assertEqual(resume[3], 't-42')
        self.assertIn('sandbox_mode=read-only', resume)
        self.assertIn('rules here', seen['recover'])
        self.assertIn(codex_agent.OUTPUT_CONTRACT + json.dumps(SCHEMA), seen['recover'])

    def test_review_prose_twice_is_a_failure(self):
        out, err, seen = self._run({'prompt': 'x', 'schema': SCHEMA, 'review': True}, 'ok',
                                   result_body='prose', recovered_body='still prose')
        self.assertIsNone(out)
        self.assertIn('even after one conversion turn', err)
        self.assertEqual(len(seen['cmds']), 2)


class BridgeTest(unittest.TestCase):
    def test_bridge_frontmatter_is_valid_yaml_with_its_guards(self):
        # An unquoted `key: value` inside the description turned the whole
        # frontmatter into a parse error once; the harness then drops the
        # tools/model/effort pins silently.
        import yaml
        text = (ROOT / '.claude' / 'agents' / 'codex-agent.md').read_text()
        meta = yaml.safe_load(text.split('---\n')[1])
        self.assertEqual(meta['name'], 'codex-agent')
        self.assertIsInstance(meta['description'], str)
        self.assertEqual((meta['tools'], meta['model'], meta['effort']), ('Bash', 'haiku', 'low'))
        self.assertIn("python3 .claude/codex-agent.py <<'CODEX_AGENT_INPUT'", text)
        self.assertNotIn('codex exec', text)


if __name__ == '__main__':
    unittest.main()
