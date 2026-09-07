#!/usr/bin/env python3
"""Board+example-matched code-size compare for PR-scoped builds.

The averaged metrics baseline (metrics-tinyusb) spans every family and example;
a scoped PR builds a subset, so comparing against it is apples-to-oranges. This
compares the intersection of (board, example) pairs present on BOTH sides,
averaged over exactly those pairs, and names what was dropped. See
docs/superpowers/specs/2026-08-19-ci-build-family-filter-design.md #code-metrics.
"""
import argparse
import glob
import json
import os
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
import metrics

# dropped (board, example) pairs named in the PR comment before it truncates
DROPPED_SHOWN = 20


def board_family(board, repo_root):
    hits = glob.glob(os.path.join(repo_root, 'hw/bsp/*/boards', board))
    return os.path.basename(os.path.dirname(os.path.dirname(hits[0]))) if hits else None


def collect(root, repo_root):
    """{(board, 'role/example'): [file entries]} from every
    **/cmake-build-<board>/metrics_by_example.json under root.

    Keyed on the BOARD, not its family. The two sides are built by
    `--one-first`, which returns all_boards[0] for a family with no
    ci_preferred_boards entry - so a PR that adds hw/bsp/<family>/boards/a_new_board
    shifts which board is built, and a family key would file the base run's sizes and
    the PR run's sizes under the same name and publish the difference between two
    unrelated MCUs as this PR's code-size impact. On the board key that mismatch lands
    in `dropped` (reported as not compared), which is the truth."""
    pairs = {}
    pat = os.path.join(root, '**', 'metrics_by_example.json')
    for f in sorted(glob.glob(pat, recursive=True)):
        board = os.path.basename(os.path.dirname(f))
        if not board.startswith('cmake-build-'):
            print(f'pair_compare: {f} not under a cmake-build-<board> dir, skipping', file=sys.stderr)
            continue
        board = board[len('cmake-build-'):]
        if not board_family(board, repo_root):
            # unknown board: the name is still a usable key, but say so - it means the
            # artifact came from a tree whose hw/bsp does not match this checkout
            print(f'pair_compare: no family for board {board}', file=sys.stderr)
        # parse into a LOCAL dict and merge only once the whole file came out clean:
        # a file that blows up half way through must drop WHOLE, or the entries read
        # before the malformation stay in the comparison while stderr says the file
        # was skipped, and a silently truncated table gets published as the verdict
        try:
            one = {}
            for ex, ent in json.load(open(f)).items():
                one.setdefault((board, ex), []).extend(ent.get('files', []))
        except (OSError, ValueError, AttributeError, TypeError) as e:
            print(f'pair_compare: unreadable {f} ({e}), skipping', file=sys.stderr)
            continue
        for k, v in one.items():
            pairs.setdefault(k, []).extend(v)
    return pairs


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--base-dir', required=True)
    ap.add_argument('--new-dir', required=True)
    ap.add_argument('--out', default='metrics_compare')
    a = ap.parse_args()
    repo_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

    base = collect(a.base_dir, repo_root)
    new = collect(a.new_dir, repo_root)
    common = sorted(set(base) & set(new))
    dropped = sorted(set(base) ^ set(new))

    if not common:
        with open(a.out + '.md', 'w') as f:
            if new and not base:
                # interim state: master has not uploaded a per-example baseline yet.
                # Blaming the PR's scoping for that sends people hunting the wrong bug
                f.write('_No per-example baseline from the base branch yet (the first '
                        'master push after this feature merges uploads it); comparison '
                        'will appear on the next push._\n')
            else:
                f.write('_Code-size comparison skipped: no (board, example) pair was '
                        'built on both the base branch and this PR._\n')
        return

    def synth(pairs, path):
        with open(path, 'w') as f:
            json.dump({'files': [e for k in common for e in pairs[k]]}, f)

    with tempfile.TemporaryDirectory() as td:
        b, n = os.path.join(td, 'base.json'), os.path.join(td, 'new.json')
        synth(base, b)
        synth(new, n)
        comparison = metrics.compare_files(b, n, ['tinyusb/src'])
        if comparison is None:
            with open(a.out + '.md', 'w') as f:
                f.write('_Code-size comparison failed to produce data._\n')
            return
        metrics.write_compare_markdown(comparison, a.out + '.md', 'name+')

    with open(a.out + '.md', 'a') as f:
        boards = sorted({k[0] for k in common})
        f.write(f'\n_Scoped compare: {len(common)} (board, example) pairs across '
                f'{", ".join(boards)}._\n')
        if dropped:
            # GitHub caps a comment at 65,536 chars and this footer rides inside the
            # sticky code-metrics comment: a broad scoped PR drops hundreds of pairs,
            # and the raw list alone reached ~65KB and reddened the whole job. Only a
            # summary goes in the comment; the full list goes to the job log.
            names = [f'{board}:{ex}' for board, ex in dropped]
            print('pair_compare: not compared (missing on one side): '
                  + ', '.join(names), file=sys.stderr)
            more = len(names) - DROPPED_SHOWN
            f.write(f'_Not compared (missing on one side): {len(names)} pairs - '
                    + ', '.join(names[:DROPPED_SHOWN])
                    + (f', ... and {more} more (see the code-metrics job log)'
                       if more > 0 else '')
                    + '._\n')


if __name__ == '__main__':
    main()
