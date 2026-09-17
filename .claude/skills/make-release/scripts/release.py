#!/usr/bin/env python3
"""Mechanics of cutting a TinyUSB release; the judgment (which version, how to
curate the changelog) stays in SKILL.md.

    release.py bump X.Y.Z            # version into tusb_option.h, repository.yml, library.json, sonar-project.properties
    release.py prs --prev X.Y.Z      # PRs merged since the previous release: '#N<TAB>title<TAB>[labels]'
    release.py contributors --prev X.Y.Z   # their non-bot authors as '@a, @b'

Both range commands read HEAD unless --head names the release branch.

Every step refuses rather than guesses: a version that is not X.Y.Z or equals
the current one, a file a substitution did not change, a --prev that is not an
ancestor of HEAD, a first-parent commit that is not a merged PR."""
import argparse
import json
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
VERSION_RE = re.compile(r'^\d+\.\d+\.\d+$')
BOT_RE = re.compile(r'\[bot\]$|^(copilot|claude|dependabot|github-actions)$', re.IGNORECASE)
MERGE_RE = re.compile(r'^Merge pull request #(\d+)')
SQUASH_RE = re.compile(r'\(#(\d+)\)$')


class Refused(Exception):
    pass


def current_version(option_h):
    parts = {k: v for k, v in re.findall(r'#define TUSB_VERSION_(MAJOR|MINOR|REVISION) +(\d+)', option_h)}
    if len(parts) != 3:
        raise Refused('TUSB_VERSION_{MAJOR,MINOR,REVISION} not all found in src/tusb_option.h')
    return f'{parts["MAJOR"]}.{parts["MINOR"]}.{parts["REVISION"]}'


def repository_edits(text, version, major):
    """The substitutions for repository.yml: the version gets its own entry above the
    aliases, unless a rerun already added it, and the alias of ITS major moves to it. A
    first release of a new major adds that alias; the previous major's keeps pointing at
    the last release of that major, since a 0-latest user did not ask for 1.x."""
    alias = f'"{major}-latest"'
    listed = f'"{version}": "{version}"' in text
    if f'{alias}:' in text:
        subs = [(rf'({re.escape(alias)}): "\d+\.\d+\.\d+"', rf'\1: "{version}"')]
        return subs if listed else [(rf'( *)({re.escape(alias)}): "\d+\.\d+\.\d+"', rf'\1"{version}": "{version}"\n\1\2: "{version}"')]
    # no alias for this major yet: put the version and the new alias above the first alias
    entry = '' if listed else f'"{version}": "{version}"\n\\1'
    return [(r'( *)("\d+-latest": "\d+\.\d+\.\d+")', rf'\1{entry}{alias}: "{version}"\n\1\2')]


def bump(root, version):
    """Substitute the version into the four release files; the changed paths, or Refused."""
    if not VERSION_RE.match(version):
        raise Refused(f'version must be X.Y.Z, got {version!r}')
    major, minor, rev = version.split('.')
    option_h = root / 'src/tusb_option.h'
    current = current_version(option_h.read_text())
    if current == version:
        raise Refused(f'{version} is already the version in src/tusb_option.h')

    edits = {
        option_h: [
            (r'(#define TUSB_VERSION_MAJOR *) \d+', rf'\g<1> {major}'),
            (r'(#define TUSB_VERSION_MINOR *) \d+', rf'\g<1> {minor}'),
            (r'(#define TUSB_VERSION_REVISION *) \d+', rf'\g<1> {rev}'),
        ],
        root / 'repository.yml': repository_edits((root / 'repository.yml').read_text(), version, major),
        root / 'library.json': [(r'( {4}"version":) "\d+\.\d+\.\d+"', rf'\1 "{version}"')],
        root / 'sonar-project.properties': [(r'(sonar\.projectVersion=)\d+\.\d+\.\d+', rf'\g<1>{version}')],
    }
    pending = []
    for path, subs in edits.items():  # validate everything first: a refusal writes nothing
        before = path.read_text()
        after = before
        for pattern, repl in subs:
            after = re.sub(pattern, repl, after, count=1)
        if after == before:
            raise Refused(f'{path.relative_to(root)}: no line matched the version pattern; nothing written')
        pending.append((path, after))
    for path, after in pending:
        path.write_text(after)
    return [path for path, _ in pending]


def git(root, *args):
    r = subprocess.run(['git', '-C', str(root), *args], capture_output=True, text=True)
    if r.returncode != 0:
        raise Refused(f'git {" ".join(args)} failed: {r.stderr.strip()}')
    return r.stdout


def pr_numbers(subjects):
    """PR numbers of first-parent merge commits (merge button or squash); Refused for anything else."""
    numbers, unmatched = set(), []
    for s in subjects:
        m = MERGE_RE.match(s) or SQUASH_RE.search(s)
        if m:
            numbers.add(int(m.group(1)))
        elif s:
            unmatched.append(s)
    if unmatched:
        raise Refused('first-parent commits that are not merged PRs (direct pushes?):\n  ' + '\n  '.join(unmatched))
    return sorted(numbers)


def merged_prs(root, prev, head='HEAD'):
    if not VERSION_RE.match(prev):
        raise Refused(f'--prev must be X.Y.Z, got {prev!r}')
    if subprocess.run(['git', '-C', str(root), 'merge-base', '--is-ancestor', prev, head],
                      capture_output=True).returncode != 0:
        raise Refused(f'{prev} is not an ancestor of {head}: wrong tag, wrong branch, or not fetched')
    return pr_numbers(git(root, 'log', '--first-parent', f'{prev}..{head}', '--pretty=%s').split('\n'))


def gh_pr(root, number, attempts=3):
    """One PR's number, title, labels and author; a few hundred calls in a row hit transient API errors."""
    for attempt in range(attempts):
        r = subprocess.run(['gh', 'pr', 'view', str(number), '--json', 'number,title,labels,author'],
                           cwd=root, capture_output=True, text=True)
        if r.returncode == 0:
            return json.loads(r.stdout)
    raise Refused(f'gh pr view {number} failed {attempts} times: {r.stderr.strip()}')


def fetch(root, numbers):
    with ThreadPoolExecutor(8) as pool:
        return list(pool.map(lambda n: gh_pr(root, n), numbers))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--root', type=Path, default=ROOT, help='repository to operate on (tests)')
    sub = p.add_subparsers(dest='cmd', required=True)
    sub.add_parser('bump', help='write the version into the four release files').add_argument('version')
    for name in ('prs', 'contributors'):
        q = sub.add_parser(name)
        q.add_argument('--prev', required=True, metavar='X.Y.Z',
                       help='the previous release tag; PRs are those reachable from --head but not from it')
        q.add_argument('--head', default='HEAD', help='the release branch (default HEAD)')
    a = p.parse_args()
    root = a.root.resolve()
    try:
        if a.cmd == 'bump':
            for path in bump(root, a.version):
                print(f'{a.version} -> {path.relative_to(root)}')
            print(f'now add docs/changelog/{a.version}.md and list it first in docs/changelog/index.rst')
            return 0
        prs = fetch(root, merged_prs(root, a.prev, a.head))
        if a.cmd == 'prs':
            for pr in prs:
                labels = ','.join(l['name'] for l in pr['labels'])
                print(f'#{pr["number"]}\t{pr["title"]}\t[{labels}]')
        else:
            authors = {pr['author']['login'] for pr in prs}
            print(', '.join(f'@{a}' for a in sorted((x for x in authors if not BOT_RE.search(x)), key=str.lower)))
        return 0
    except Refused as e:
        print(f'release: {e}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
