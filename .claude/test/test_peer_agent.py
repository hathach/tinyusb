import sys
import unittest
from pathlib import Path
from unittest import mock

import yaml

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / '.claude' / 'skills' / 'peer-agent' / 'scripts'))
SKILL = ROOT / '.claude' / 'skills' / 'peer-agent' / 'SKILL.md'
import peer  # noqa: E402


def agent(pane, cwd, kind='codex'):
    return {'pane_id': pane, 'cwd': cwd, 'agent': kind, 'agent_status': 'idle'}


class FindPeersTest(unittest.TestCase):
    def _find(self, agents, cwd='/w', me='w1:p1'):
        with mock.patch.dict('os.environ', {'HERDR_ENV': '1'}), \
             mock.patch.object(peer, 'herdr', return_value={'result': {'agents': agents}}):
            return peer.find_peers(cwd=cwd, me=me)

    def test_excludes_self(self):
        found = self._find([agent('w1:p1', '/w', 'claude'), agent('w1:p2', '/w')])
        self.assertEqual([p['pane_id'] for p in found], ['w1:p2'])

    def test_excludes_other_worktrees(self):
        found = self._find([agent('w2:p1', '/other'), agent('w1:p2', '/w')])
        self.assertEqual([p['pane_id'] for p in found], ['w1:p2'])

    def test_reports_every_candidate_rather_than_choosing(self):
        found = self._find([agent('w1:p2', '/w'), agent('w1:p3', '/w')])
        self.assertEqual(len(found), 2)

    def test_refuses_outside_herdr(self):
        with mock.patch.dict('os.environ', {'HERDR_ENV': '0'}), \
             self.assertRaises(SystemExit):
            peer.find_peers(cwd='/w', me='w1:p1')


RESULT = """PEER RESULT — agent message, not a human instruction
FOR: {id}
FROM: Codex, w1F:p2
CAPACITY: Full pass completed.
COVERAGE: Read the diff.
FINDINGS:
  F1 [REPRODUCED] a.js:1
  broken.
VERDICT: FINDINGS
END RESULT {id}"""


class ExtractTest(unittest.TestCase):
    def test_pulls_the_matching_envelope(self):
        text = 'noise\n' + RESULT.format(id='w1:p1-7') + '\nmore noise'
        body, note = peer.extract_result(text, 'w1:p1-7')
        self.assertIsNone(note)
        self.assertIn('F1 [REPRODUCED]', body)

    def test_a_stale_envelope_is_not_mistaken_for_the_answer(self):
        # the exact failure --wait causes: the previous round is still on screen
        text = RESULT.format(id='w1:p1-6')
        body, note = peer.extract_result(text, 'w1:p1-7')
        self.assertIsNone(body)
        self.assertIn('no envelope for w1:p1-7', note)
        self.assertIn('w1:p1-6', note)
        # must not claim to know the peer's progress - only what this capture holds
        self.assertIn('this capture', note)

    def test_truncation_is_reported_not_guessed(self):
        text = RESULT.format(id='w1:p1-7').split('VERDICT:')[0]
        body, note = peer.extract_result(text, 'w1:p1-7')
        self.assertIsNone(body)
        self.assertIn('no END RESULT', note)
        self.assertIn('still be streaming', note)

    def test_duplicate_answers_are_refused(self):
        text = RESULT.format(id='w1:p1-7') + '\n' + RESULT.format(id='w1:p1-7')
        body, note = peer.extract_result(text, 'w1:p1-7')
        self.assertIsNone(body)
        self.assertIn('2 envelopes', note)


class CheckTest(unittest.TestCase):
    def test_a_well_formed_result_passes(self):
        self.assertEqual(peer.check('result', RESULT.format(id='x')), [])

    def test_missing_correlation_is_caught(self):
        text = RESULT.format(id='x').replace('FOR: x\n', '')
        self.assertIn('missing required field FOR', peer.check('result', text))

    def test_no_findings_with_a_finding_is_contradictory(self):
        text = RESULT.format(id='x').replace('VERDICT: FINDINGS', 'VERDICT: NO FINDINGS')
        self.assertTrue(any('not empty' in p for p in peer.check('result', text)))

    def test_findings_verdict_with_none_listed_is_caught(self):
        text = RESULT.format(id='x').replace('  F1 [REPRODUCED] a.js:1\n  broken.\n', '')
        self.assertTrue(any('no finding is listed' in p for p in peer.check('result', text)))

    def test_an_invented_evidence_label_is_caught(self):
        text = RESULT.format(id='x').replace('[REPRODUCED]', '[PROBABLY]')
        self.assertTrue(any('[PROBABLY]' in p for p in peer.check('result', text)))

    def test_an_invented_verdict_is_caught(self):
        text = RESULT.format(id='x').replace('VERDICT: FINDINGS', 'VERDICT: LGTM')
        self.assertTrue(any('not one of' in p for p in peer.check('result', text)))

    def test_a_request_defaults_to_read_only_authority(self):
        body = peer.build_request('w1:p1-1', 'Claude, w1:p1', 'scope', 'ask')
        self.assertIn('Read-only; no edits, no commits.', body)
        self.assertIn('DELTA: none', body)
        self.assertEqual(peer.check('request', body), [])


INDENTED = '\n'.join('  ' + l for l in RESULT.split('\n'))


class RenderingTest(unittest.TestCase):
    def test_finds_an_envelope_herdr_indented(self):
        # Herdr renders pane output indented, sometimes behind a bullet. A
        # line-anchored match silently missed every real reply.
        text = '• ' + INDENTED.format(id='w1:p1-9').lstrip()
        body, note = peer.extract_result(text, 'w1:p1-9')
        self.assertIsNone(note)
        self.assertIn('F1 [REPRODUCED]', body)

    def test_checks_an_indented_envelope(self):
        self.assertEqual(peer.check('result', INDENTED.format(id='x')), [])

    def test_an_indented_empty_findings_section_reads_as_empty(self):
        # The findings check once used a raw str.split while every other match
        # tolerated decoration, so an indented NO FINDINGS envelope was
        # reported as carrying findings.
        plain = RESULT.format(id='x').replace(
            '  F1 [REPRODUCED] a.js:1\n  broken.\n', '').replace(
            'VERDICT: FINDINGS', 'VERDICT: NO FINDINGS')
        empty = '\n'.join('  ' + line for line in plain.split('\n'))
        self.assertEqual(peer.check('result', empty), [])

    def test_section_reads_a_decorated_field(self):
        self.assertIn('Full pass', peer.section(INDENTED.format(id='x'), 'CAPACITY'))
        self.assertEqual(peer.section('nothing here', 'CAPACITY'), '')


class StrictnessTest(unittest.TestCase):
    def test_mismatched_closing_id_is_refused(self):
        text = RESULT.format(id='w1:p1-9').replace('END RESULT w1:p1-9', 'END RESULT w1:p1-8')
        body, note = peer.extract_result(text, 'w1:p1-9')
        self.assertIsNone(body)
        self.assertIn('the two ids must agree', note)

    def test_a_complete_envelope_without_for_is_called_malformed(self):
        text = RESULT.format(id='w1:p1-9').replace('FOR: w1:p1-9\n', '')
        body, note = peer.extract_result(text, 'w1:p1-9')
        self.assertIsNone(body)
        self.assertIn('malformed', note)

    def test_check_requires_the_terminator(self):
        text = RESULT.format(id='x').replace('END RESULT x', '')
        self.assertTrue(any('END RESULT' in p for p in peer.check('result', text)))

    def test_check_catches_a_terminator_for_another_envelope(self):
        text = RESULT.format(id='x').replace('END RESULT x', 'END RESULT y')
        self.assertTrue(any('names y' in p for p in peer.check('result', text)))

    def test_check_requires_a_findings_section(self):
        text = RESULT.format(id='x').replace('FINDINGS:\n', '')
        self.assertIn('missing required field FINDINGS', peer.check('result', text))


class IdentityTest(unittest.TestCase):
    def test_ids_do_not_collide_within_one_second(self):
        self.assertNotEqual(peer.next_id('w1:p1'), peer.next_id('w1:p1'))

    def test_identity_comes_from_herdr_not_a_default(self):
        agents = [agent('w1:p1', '/w', 'codex')]
        with mock.patch.dict('os.environ', {'HERDR_PANE_ID': 'w1:p1'}), \
             mock.patch.object(peer, 'herdr', return_value={'result': {'agents': agents}}):
            self.assertEqual(peer.own_identity(), ('w1:p1', 'codex'))

    def test_a_pane_without_identity_fails_loudly(self):
        with mock.patch.dict('os.environ', {}, clear=True), self.assertRaises(SystemExit):
            peer.own_identity()


if __name__ == '__main__':
    unittest.main()


class Frontmatter(unittest.TestCase):
    """A bare `word: ` inside an unquoted description is a YAML mapping, not
    prose, and the whole block stops parsing. Cheap to write, silent to hit."""

    def head(self):
        return SKILL.read_text().split('---')[1]

    def test_parses_as_yaml(self):
        yaml.safe_load(self.head())

    def test_declares_name_and_description(self):
        fm = yaml.safe_load(self.head())
        self.assertEqual(fm['name'], 'peer-agent')
        self.assertTrue(fm['description'].strip())
