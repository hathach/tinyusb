#!/usr/bin/env python3
"""pr_comment.yml's code-size comment mechanics. Needs `gh` with a token that can read Actions.

  code_size_ci.py baseline --repo OWNER/NAME --current CUR_DIR --info INFO.json

finds the baseline of a PR's Build run: the master push Build run of the commit the PR
was built against (its merge commit's first parent), else of its nearest first-parent
ancestor with snapshots, labelled approximate. Never rebuilds; no baseline, a failed
lookup included, is a result, not an error. CUR_DIR holds the PR run's downloaded
code-size artifacts; INFO.json gets {sha, url, exact, note, run_id} for `tools/code_size.py
compare --baseline-info`, sha and run_id None when there is no baseline. The workflow
downloads run_id's `code-size-*` artifacts.

  code_size_ci.py is-current --repo OWNER/NAME --pr N --run-id ID --attempt A --head-sha SHA

exits 0 when that Build run attempt is still the PR's latest, 1 when a newer push, run
or attempt supersedes it: its comment would overwrite a newer one; 2 when the API, or
agentrc's gh_pr_comment.py that answers it, cannot tell.
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'tools'))
import code_size  # noqa: E402  its engine's snapshot loader is the one compare uses
from helper import agentrc  # noqa: E402  test/hil/helper, on the path code_size.py adds

WORKFLOW = 'build.yml'
GH_PR_COMMENT = 'skills/hil/scripts/gh_pr_comment.py'   # agentrc's, for is-current
BRANCH = 'master'
MAX_DEPTH = 30  # first-parent ancestors tried before giving up


def gh(path, paginate_key=None):
    """agentrc's gh_pr_comment.gh: the parsed `gh api` GET of `path`, with `paginate_key` the items under it on
    every page; RuntimeError when the API, or agentrc, cannot answer."""
    try:
        helper = agentrc.load_file(GH_PR_COMMENT)
    except ImportError as e:
        raise RuntimeError(str(e)) from None
    try:
        return helper.gh(path, paginate_key)
    except helper.ApiError as e:
        raise RuntimeError(str(e)) from None


def gh_items(path, key):
    """Every item of a paginated list endpoint whose items are under `key`."""
    sep = '&' if '?' in path else '?'
    return gh(f'{path}{sep}per_page=100', key)


def current_base_sha(current):
    """The one base_sha the PR run's legs record, else None."""
    engine = code_size.load_engine()
    shas = {engine._commits(leg)[1] for leg in engine.load_snapshots(current)['legs'].values()}
    return shas.pop() if len(shas) == 1 else None


def is_ancestor_on(repo, sha, branch=BRANCH):
    """Whether `sha` is on `branch`'s history, so a PR can never pick its own run as baseline."""
    return gh(f'repos/{repo}/compare/{sha}...{branch}')['status'] in ('ahead', 'identical')


def has_snapshots(names):
    """Whether a run's unexpired artifact `names` hold code-size snapshots: a docs-only commit uploads a scope that
    measured nothing, not a baseline."""
    sizes = {n for n in names if n.startswith('code-size-')}
    return 'code-size-scope' in sizes and len(sizes) > 1


def snapshot_run(repo, sha, branch=BRANCH, usable=has_snapshots):
    """The latest completed `branch` push Build run of `sha` whose unexpired artifact names are `usable`, else None."""
    runs = gh_items(f'repos/{repo}/actions/workflows/{WORKFLOW}/runs?head_sha={sha}&event=push'
                    f'&branch={branch}&status=completed', 'workflow_runs')
    for run in sorted(runs, key=lambda r: r['run_number'], reverse=True):
        if run['head_sha'] != sha or run['head_branch'] != branch or run['event'] != 'push' \
                or (run.get('head_repository') or {}).get('full_name') != repo:
            continue
        names = {a['name'] for a in gh_items(f'repos/{repo}/actions/runs/{run["id"]}/artifacts', 'artifacts')
                 if not a['expired']}
        if usable(names):
            return run
    return None


def find_baseline(repo, sha, branch=BRANCH, usable=has_snapshots):
    """(run, depth) of the first first-parent ancestor of `sha`, itself at depth 0, with
    usable artifacts, or None within MAX_DEPTH."""
    for depth in range(MAX_DEPTH):
        run = snapshot_run(repo, sha, branch, usable)
        if run:
            return run, depth
        parents = gh(f'repos/{repo}/commits/{sha}')['parents']
        if not parents:
            return None
        sha = parents[0]['sha']
    return None


def lookup(repo, current):
    """The baseline description written to INFO.json."""
    sha = current_base_sha(current)
    if sha is None:
        return {'sha': None, 'url': None, 'exact': False, 'run_id': None,
                'note': 'the PR snapshots record no single base commit'}
    return lookup_sha(repo, sha)


def lookup_sha(repo, sha, whose='this PR\'s', branch=BRANCH, usable=has_snapshots):
    """The baseline description of base commit `sha`: its `branch` Build run with usable artifacts,
    else its nearest first-parent ancestor's (approximate), whose later changes count as
    `whose`; 'sha' None with a 'note' when there is none. Raises RuntimeError on an API failure."""
    info = {'sha': None, 'url': None, 'exact': False, 'run_id': None}
    if not is_ancestor_on(repo, sha, branch):
        return {**info, 'note': f'base {sha[:10]} is not on {branch}'}
    found = find_baseline(repo, sha, branch, usable)
    if found is None:
        return {**info, 'note': f'no {branch} Build run with usable artifacts within {MAX_DEPTH} commits of {sha[:10]}'}
    run, depth = found
    info = {'sha': run['head_sha'], 'url': run['html_url'], 'exact': depth == 0, 'run_id': run['id']}
    note = f'{depth} {branch} commits before this build\'s base {sha[:10]}: their changes count as {whose}'
    return {**info, 'note': note} if depth else info


def baseline(args):
    try:
        info = lookup(args.repo, args.current)
    except (RuntimeError, KeyError, TypeError, ValueError, ImportError) as e:  # API, reply or snapshot-engine failure
        info = {'sha': None, 'url': None, 'exact': False, 'run_id': None, 'note': f'the baseline lookup failed: {e}'}
    with open(args.info, 'w', encoding='utf-8') as f:
        json.dump(info, f, indent=1, sort_keys=True)
        f.write('\n')
    print(json.dumps(info))
    return 0


def is_current(args):
    """Whether run `args.run_id` attempt `args.attempt` of the PR head is still the newest Build run: agentrc's
    gh_pr_comment.fresh, by pull_requests (every PR a run lists) or a fork's head. Exit 2 when it cannot tell."""
    try:
        helper = agentrc.load_file(GH_PR_COMMENT)
    except ImportError as e:
        print(f'unknown: {e}', file=sys.stderr)
        return 2
    try:
        # api read per call, so a test stubs helper.gh
        ok, why = helper.fresh(args.repo, pr=args.pr, head_sha=args.head_sha, run_id=args.run_id,
                               attempt=args.attempt, workflow=WORKFLOW, api=helper.gh)
    except helper.ApiError as e:
        print(f'unknown: {e}', file=sys.stderr)
        return 2
    print(why if ok else f'stale: {why}')
    return 0 if ok else 1


def main():
    top = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = top.add_subparsers(dest='command', required=True)
    p = sub.add_parser('baseline', help='find the baseline run of a PR run\'s snapshots')
    p.add_argument('--repo', required=True, help='OWNER/NAME whose master runs are baselines')
    p.add_argument('--current', required=True, help='The PR run\'s downloaded code-size artifacts')
    p.add_argument('--info', required=True, help='Where to write the baseline description JSON')
    p = sub.add_parser('is-current', help='exit 1 when a newer push, run or attempt supersedes this one, '
                                          '2 when it cannot tell')
    p.add_argument('--repo', required=True)
    p.add_argument('--pr', required=True, type=int)
    p.add_argument('--run-id', required=True, type=int)
    p.add_argument('--attempt', required=True, type=int)
    p.add_argument('--head-sha', required=True)
    args = top.parse_args()
    return baseline(args) if args.command == 'baseline' else is_current(args)


if __name__ == '__main__':
    sys.exit(main())
