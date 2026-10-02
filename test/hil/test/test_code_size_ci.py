#!/usr/bin/env python3
"""Tests for .github/scripts/code_size_ci.py, against a stubbed GitHub API."""
import contextlib
import io
import json
import os
import re
import sys
import tempfile
import unittest
from unittest import mock

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
sys.path.insert(0, os.path.join(REPO, '.github', 'scripts'))
import code_size_ci as ci  # noqa: E402

NAME = 'hathach/tinyusb'


def sha(c):
    return c * 40


def run(run_id, commit, number=None, event='push', branch='master', repo=NAME):
    return {'id': run_id, 'run_number': number or run_id, 'head_sha': commit, 'head_branch': branch,
            'event': event, 'head_repository': {'full_name': repo},
            'html_url': f'https://github.com/{NAME}/actions/runs/{run_id}'}


def artifact(art_id, name, expired=False):
    return {'id': art_id, 'name': name, 'expired': expired}


class FakeGh:
    """The API paths code_size_ci.py reads: first-parent history, runs per commit,
    artifacts per run, the ancestry compare, a PR and a run. A paginated call gets
    everything as one page."""

    def __init__(self, parents, runs=None, artifacts=None, status='ahead'):
        self.parents, self.runs, self.artifacts, self.status = parents, runs or {}, artifacts or {}, status
        self.pr_head, self.pr_repo, self.attempts = None, NAME, {}
        self.pr_state, self.pr_base = 'open', 'master'

    def __call__(self, path, paginate=False):
        reply = self.reply(path)
        return [reply] if paginate else reply

    def reply(self, path):
        if m := re.match(rf'repos/{NAME}/actions/workflows/build\.yml/runs\?head_sha=(\w+)&', path):
            return {'workflow_runs': self.runs.get(m.group(1), [])}
        if m := re.fullmatch(rf'repos/{NAME}/actions/runs/(\d+)/artifacts', path):
            return {'artifacts': self.artifacts.get(int(m.group(1)), [])}
        if m := re.fullmatch(rf'repos/{NAME}/actions/runs/(\d+)', path):
            return {'run_attempt': self.attempts[int(m.group(1))]}
        if m := re.fullmatch(rf'repos/{NAME}/commits/(\w+)', path):
            parent = self.parents.get(m.group(1))
            return {'parents': [{'sha': parent}] if parent else []}
        if re.fullmatch(rf'repos/{NAME}/pulls/\d+', path):
            return {'head': {'sha': self.pr_head, 'ref': 'feature', 'repo': {'full_name': self.pr_repo}},
                    'state': self.pr_state, 'base': {'ref': self.pr_base}}
        if re.fullmatch(rf'repos/{NAME}/compare/\w+\.\.\.master', path):
            return {'status': self.status}
        raise AssertionError(f'unexpected API call {path}')


SNAPSHOTS = [artifact(1, 'code-size-scope'), artifact(2, 'code-size-arm-gcc-stm32f4')]


def leg(base):
    return {'schema': 1, 'boards': [], 'examples': None, 'build_outcome': 'success',
            'sha': sha('e'), 'base_sha': base, 'head_sha': sha('f')}


class Baseline(unittest.TestCase):
    def lookup(self, fake, base=sha('b'), legs=None):
        with tempfile.TemporaryDirectory() as tmp:
            cur, info = os.path.join(tmp, 'cur'), os.path.join(tmp, 'info.json')
            for i, data in enumerate(legs or [leg(base)]):
                os.makedirs(os.path.join(cur, f'code-size-leg{i}'))
                with open(os.path.join(cur, f'code-size-leg{i}', 'leg.json'), 'w') as f:
                    json.dump(data, f)
            args = ['code_size_ci.py', 'baseline', '--repo', NAME, '--current', cur, '--info', info]
            with mock.patch.object(ci, 'gh', fake), mock.patch.object(sys, 'argv', args), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(ci.main(), 0)
            with open(info) as f:
                return json.load(f)

    def test_the_base_commits_own_run_is_exact(self):
        fake = FakeGh({}, {sha('b'): [run(7, sha('b'))]}, {7: SNAPSHOTS})
        self.assertEqual(self.lookup(fake), {'sha': sha('b'), 'url': f'https://github.com/{NAME}/actions/runs/7',
                                             'exact': True, 'run_id': 7})

    def test_a_commit_without_snapshots_falls_back_to_its_first_parent_as_approximate(self):
        docs_only = [artifact(3, 'code-size-scope'), artifact(4, 'binaries-arm-gcc-x')]
        fake = FakeGh({sha('b'): sha('a')}, {sha('b'): [run(8, sha('b'))], sha('a'): [run(7, sha('a'))]},
                      {8: docs_only, 7: SNAPSHOTS})
        info = self.lookup(fake)
        self.assertEqual((info['sha'], info['exact'], info['run_id']), (sha('a'), False, 7))
        self.assertIn('1 master commits before', info['note'])

    def test_a_local_caller_names_whose_changes_the_gap_counts_as(self):
        fake = FakeGh({sha('b'): sha('a')}, {sha('a'): [run(7, sha('a'))]}, {7: SNAPSHOTS})
        with mock.patch.object(ci, 'gh', fake):
            self.assertTrue(ci.lookup_sha(NAME, sha('b'), 'yours')['note'].endswith('their changes count as yours'))

    def test_a_stalled_gh_call_is_an_api_failure(self):
        with mock.patch.object(ci.subprocess, 'run', side_effect=ci.subprocess.TimeoutExpired('gh', 60)), \
             self.assertRaisesRegex(RuntimeError, 'no reply in 60 s'):
            ci.gh('repos/x')

    def test_expired_artifacts_do_not_count(self):
        expired = [artifact(1, 'code-size-scope'), artifact(2, 'code-size-arm-gcc-x', expired=True)]
        fake = FakeGh({sha('b'): sha('a')}, {sha('b'): [run(8, sha('b'))], sha('a'): [run(7, sha('a'))]},
                      {8: expired, 7: SNAPSHOTS})
        self.assertEqual(self.lookup(fake)['sha'], sha('a'))

    def test_the_latest_run_of_a_commit_wins(self):
        fake = FakeGh({}, {sha('b'): [run(7, sha('b')), run(8, sha('b'))]}, {7: SNAPSHOTS, 8: SNAPSHOTS})
        self.assertEqual(self.lookup(fake)['run_id'], 8)

    def test_runs_of_other_events_branches_or_repos_are_not_baselines(self):
        others = [run(5, sha('b'), event='pull_request'), run(6, sha('b'), branch='dev'),
                  run(7, sha('b'), repo='fork/tinyusb')]
        fake = FakeGh({}, {sha('b'): others}, {5: SNAPSHOTS, 6: SNAPSHOTS, 7: SNAPSHOTS})
        self.assertIsNone(self.lookup(fake)['run_id'])

    def test_a_base_off_master_has_no_baseline(self):
        fake = FakeGh({}, {sha('b'): [run(7, sha('b'))]}, {7: SNAPSHOTS}, status='diverged')
        info = self.lookup(fake)
        self.assertIsNone(info['run_id'])
        self.assertIn('not on master', info['note'])

    def test_the_search_stops_at_the_depth_cap(self):
        history = [f'{i:040x}' for i in range(ci.MAX_DEPTH + 5)]
        fake = FakeGh(dict(zip(history, history[1:])), {history[-1]: [run(7, history[-1])]}, {7: SNAPSHOTS})
        info = self.lookup(fake, base=history[0])
        self.assertIsNone(info['sha'])
        self.assertIn(f'within {ci.MAX_DEPTH} commits', info['note'])

    def test_legs_that_disagree_on_their_base_or_are_invalid_have_no_baseline(self):
        for legs in ([leg(sha('b')), leg(sha('c'))], [leg(['x'])], [{'base_sha': sha('b')}]):
            info = self.lookup(FakeGh({}), legs=legs)
            self.assertIsNone(info['sha'])
            self.assertIn('no single base commit', info['note'])

    def test_a_failed_lookup_is_reported_not_raised(self):
        def broken(path, paginate=False):
            raise RuntimeError('gh api: HTTP 502')
        info = self.lookup(broken)
        self.assertEqual((info['sha'], info['run_id']), (None, None))
        self.assertIn('the baseline lookup failed: gh api: HTTP 502', info['note'])


def pr_run(run_id, prs=(5,), branch='feature', repo=NAME, status='completed', conclusion='failure'):
    return {**run(run_id, sha('c'), event='pull_request', branch=branch, repo=repo),
            'pull_requests': [{'number': n} for n in prs], 'status': status, 'conclusion': conclusion,
            'run_attempt': 2}


class IsCurrent(unittest.TestCase):
    def check(self, pr_head=sha('c'), runs=(pr_run(7),), attempts=None, pr_repo=NAME):
        fake = FakeGh({}, {sha('c'): list(runs)})
        fake.pr_head, fake.pr_repo, fake.attempts = pr_head, pr_repo, attempts or {7: 1}
        args = ['code_size_ci.py', 'is-current', '--repo', NAME, '--pr', '5', '--run-id', '7',
                '--attempt', '1', '--head-sha', sha('c')]
        out = io.StringIO()
        with mock.patch.object(ci, 'gh', fake), mock.patch.object(sys, 'argv', args), \
             contextlib.redirect_stdout(out):
            return ci.main(), out.getvalue().strip()

    def test_the_latest_attempt_of_the_latest_run_of_the_head_is_current(self):
        self.assertEqual(self.check(), (0, 'current'))

    def test_a_moved_head_a_newer_run_or_attempt_is_stale(self):
        self.assertEqual(self.check(pr_head=sha('d')), (1, 'stale: the PR head moved on'))
        self.assertEqual(self.check(runs=(pr_run(7), pr_run(8))), (1, 'stale: a newer Build run exists: run 8 of #5'))
        self.assertEqual(self.check(attempts={7: 2}), (1, 'stale: a newer attempt exists'))
        self.assertEqual(self.check(runs=()), (1, 'stale: no Build run of the PR head found'))

    def test_a_newer_run_of_another_pr_sharing_the_head_does_not_supersede(self):
        self.assertEqual(self.check(runs=(pr_run(7), pr_run(8, prs=(6,)))), (0, 'current'))
        self.assertEqual(self.check(runs=(pr_run(7), pr_run(8, prs=(5, 6)))),
                         (1, 'stale: a newer Build run exists: run 8 of #5, #6'))

    def test_a_fork_prs_unlisted_runs_count_by_their_head_branch_and_repo(self):
        fork = 'someone/tinyusb'
        own = pr_run(7, prs=(), repo=fork)
        self.assertEqual(self.check(runs=(own,), pr_repo=fork), (0, 'current'))
        self.assertEqual(self.check(runs=(own, pr_run(8, prs=(), repo=fork)), pr_repo=fork),
                         (1, 'stale: a newer Build run exists: run 8 of someone/tinyusb:feature'))
        self.assertEqual(self.check(runs=(own, pr_run(8, prs=(), branch='other', repo=fork)), pr_repo=fork),
                         (0, 'current'))
        self.assertEqual(self.check(runs=(own, pr_run(8, prs=(), repo='other/tinyusb')), pr_repo=fork),
                         (0, 'current'))


class PrRun(unittest.TestCase):
    def resolve(self, runs=(pr_run(7),), state='open', base='master', pr_repo=NAME):
        fake = FakeGh({}, {sha('c'): list(runs)})
        fake.pr_head, fake.pr_repo, fake.pr_state, fake.pr_base = sha('c'), pr_repo, state, base
        args = ['code_size_ci.py', 'pr-run', '--repo', NAME, '--pr', '5']
        out, err = io.StringIO(), io.StringIO()
        with mock.patch.object(ci, 'gh', fake), mock.patch.object(sys, 'argv', args), \
             contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            return ci.main(), out.getvalue(), err.getvalue().strip()

    def test_the_newest_completed_run_of_the_head_is_printed_for_github_output(self):
        self.assertEqual(self.resolve(runs=(pr_run(7), pr_run(8, prs=(6,)))),
                         (0, f'number=5\nrun_id=7\nrun_attempt=2\nhead_sha={sha("c")}\n', ''))

    def test_a_newer_unfinished_or_a_cancelled_run_refuses_rather_than_using_an_older_one(self):
        self.assertEqual(self.resolve(runs=(pr_run(7), pr_run(8, status='in_progress'))),
                         (1, '', 'no refresh: run 8 of #5 is in_progress: its completion posts the comment'))
        self.assertEqual(self.resolve(runs=(pr_run(8, conclusion='cancelled'),)),
                         (1, '', 'no refresh: run 8 of #5 was cancelled'))
        self.assertEqual(self.resolve(runs=()), (1, '', 'no refresh: no Build run of the PR head found'))

    def test_a_closed_pr_or_one_not_targeting_master_refuses(self):
        self.assertEqual(self.resolve(state='closed'), (1, '', 'no refresh: PR #5 is closed'))
        self.assertEqual(self.resolve(base='dev'), (1, '', 'no refresh: PR #5 targets dev, not master'))

    def test_a_fork_prs_unlisted_run_is_found_by_its_head_branch_and_repo(self):
        fork = 'someone/tinyusb'
        code, out, _ = self.resolve(runs=(pr_run(7, prs=(), repo=fork), pr_run(8, prs=(), repo='other/tinyusb')),
                                    pr_repo=fork)
        self.assertEqual((code, out.splitlines()[1]), (0, 'run_id=7'))


if __name__ == '__main__':
    unittest.main()
