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
or attempt supersedes it: its comment would overwrite a newer one.
"""
import argparse
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'tools'))
import code_size  # noqa: E402  stdlib-only; its snapshot loader is the one compare uses

WORKFLOW = 'build.yml'
BRANCH = 'master'
MAX_DEPTH = 30  # first-parent ancestors tried before giving up
GH_TIMEOUT = 60  # seconds per gh api call, a stalled one never blocks


def gh(path, paginate=False):
    """Parsed `gh api` GET of `path`; with `paginate`, the list of every page."""
    cmd = ['gh', 'api', '-H', 'Accept: application/vnd.github+json'] + (['--paginate', '--slurp'] if paginate else [])
    try:
        ret = subprocess.run(cmd + [path], capture_output=True, text=True, check=False, timeout=GH_TIMEOUT)
    except subprocess.TimeoutExpired:
        raise RuntimeError(f'gh api {path}: no reply in {GH_TIMEOUT} s') from None
    if ret.returncode != 0:
        raise RuntimeError(f'gh api {path}: {ret.stderr.strip()}')
    return json.loads(ret.stdout)


def gh_items(path, key):
    """Every item of a paginated list endpoint whose items are under `key`."""
    return [item for page in gh(path, paginate=True) for item in page[key]]


def current_base_sha(current):
    """The one base_sha the PR run's legs record, else None."""
    shas = {code_size._commits(leg)[1] for leg in code_size.load_snapshots(current)['legs'].values()}
    return shas.pop() if len(shas) == 1 else None


def is_master_ancestor(repo, sha):
    """Whether `sha` is on master's history, so a PR can never pick its own run as baseline."""
    return gh(f'repos/{repo}/compare/{sha}...{BRANCH}')['status'] in ('ahead', 'identical')


def snapshot_run(repo, sha):
    """The latest completed master push Build run of `sha` with unexpired snapshots, else None."""
    runs = gh_items(f'repos/{repo}/actions/workflows/{WORKFLOW}/runs?head_sha={sha}&event=push'
                    f'&branch={BRANCH}&status=completed', 'workflow_runs')
    for run in sorted(runs, key=lambda r: r['run_number'], reverse=True):
        if run['head_sha'] != sha or run['head_branch'] != BRANCH or run['event'] != 'push' \
                or (run.get('head_repository') or {}).get('full_name') != repo:
            continue
        names = {a['name'] for a in gh_items(f'repos/{repo}/actions/runs/{run["id"]}/artifacts', 'artifacts')
                 if a['name'].startswith('code-size-') and not a['expired']}
        # a docs-only commit uploads a scope that measured nothing: not a baseline
        if 'code-size-scope' in names and len(names) > 1:
            return run
    return None


def find_baseline(repo, sha):
    """(run, depth) of the first first-parent ancestor of `sha`, itself at depth 0, with
    snapshots, or None within MAX_DEPTH."""
    for depth in range(MAX_DEPTH):
        run = snapshot_run(repo, sha)
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


def lookup_sha(repo, sha, whose='this PR\'s'):
    """The baseline description of base commit `sha`: its master Build run with snapshots,
    else its nearest first-parent ancestor's (approximate), whose later changes count as
    `whose`; 'sha' None with a 'note' when there is none. Raises RuntimeError on an API failure."""
    info = {'sha': None, 'url': None, 'exact': False, 'run_id': None}
    if not is_master_ancestor(repo, sha):
        return {**info, 'note': f'base {sha[:10]} is not on {BRANCH}'}
    found = find_baseline(repo, sha)
    if found is None:
        return {**info, 'note': f'no {BRANCH} Build run with snapshots within {MAX_DEPTH} commits of {sha[:10]}'}
    run, depth = found
    info = {'sha': run['head_sha'], 'url': run['html_url'], 'exact': depth == 0, 'run_id': run['id']}
    note = f'{depth} {BRANCH} commits before this build\'s base {sha[:10]}: their changes count as {whose}'
    return {**info, 'note': note} if depth else info


def baseline(args):
    try:
        info = lookup(args.repo, args.current)
    except (RuntimeError, KeyError, TypeError, ValueError) as e:  # an API failure or an unexpected reply
        info = {'sha': None, 'url': None, 'exact': False, 'run_id': None, 'note': f'the baseline lookup failed: {e}'}
    with open(args.info, 'w') as f:
        json.dump(info, f, indent=1, sort_keys=True)
        f.write('\n')
    print(json.dumps(info))
    return 0


def is_run_of(run, pr, head):
    """Whether a pull_request Build run may be PR `pr`'s, whose head is `head`: another PR can
    share the commit. GitHub lists the open same-repo PRs of the run's head in pull_requests and
    leaves it empty for a fork's, so an unlisted run counts when it built the PR's head branch."""
    numbers = [p['number'] for p in run.get('pull_requests') or []]
    if numbers:
        return pr in numbers
    return run.get('head_branch') == head.get('ref') and \
        (run.get('head_repository') or {}).get('full_name') == (head.get('repo') or {}).get('full_name')


def run_owner(run):
    """Run `run`'s id and whose it is, so a stale verdict names what superseded it."""
    numbers = [p['number'] for p in run.get('pull_requests') or []]
    owner = ', '.join(f'#{n}' for n in numbers) if numbers else \
        f"{(run.get('head_repository') or {}).get('full_name')}:{run.get('head_branch')}"
    return f"run {run['id']} of {owner}"


def is_current(args):
    """Whether run `args.run_id` attempt `args.attempt` of the PR head is still the newest."""
    why = None
    head = gh(f'repos/{args.repo}/pulls/{args.pr}')['head']
    if head['sha'] != args.head_sha:
        why = 'the PR head moved on'
    else:
        runs = [r for r in gh_items(f'repos/{args.repo}/actions/workflows/{WORKFLOW}/runs?head_sha={args.head_sha}'
                                    f'&event=pull_request', 'workflow_runs') if is_run_of(r, args.pr, head)]
        latest = max(runs, key=lambda r: r['run_number'], default=None)
        if latest is None:
            why = 'no Build run of the PR head found'
        elif latest['id'] != args.run_id:
            why = f'a newer Build run exists: {run_owner(latest)}'
        elif gh(f'repos/{args.repo}/actions/runs/{args.run_id}')['run_attempt'] != args.attempt:
            why = 'a newer attempt exists'
    print(f'stale: {why}' if why else 'current')
    return 1 if why else 0


def main():
    top = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = top.add_subparsers(dest='command', required=True)
    p = sub.add_parser('baseline', help='find the baseline run of a PR run\'s snapshots')
    p.add_argument('--repo', required=True, help='OWNER/NAME whose master runs are baselines')
    p.add_argument('--current', required=True, help='The PR run\'s downloaded code-size artifacts')
    p.add_argument('--info', required=True, help='Where to write the baseline description JSON')
    p = sub.add_parser('is-current', help='exit 1 when a newer push, run or attempt supersedes this one')
    p.add_argument('--repo', required=True)
    p.add_argument('--pr', required=True, type=int)
    p.add_argument('--run-id', required=True, type=int)
    p.add_argument('--attempt', required=True, type=int)
    p.add_argument('--head-sha', required=True)
    args = top.parse_args()
    return baseline(args) if args.command == 'baseline' else is_current(args)


if __name__ == '__main__':
    sys.exit(main())
