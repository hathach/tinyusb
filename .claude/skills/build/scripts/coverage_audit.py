#!/usr/bin/env python3
"""Audit tools/ci_select.py's build selection against the real build graph. Audit only:
nothing in selection reads its output.

  coverage_audit.py graph --root CHECKOUT --out DIR (--board B... | --boards-file F) [--force]
  coverage_audit.py replay --graph DIR --out FILE.json (--since DATE | --commits SHA...) [--ref REF]

`graph` builds every example of each board with tools/build.py in CHECKOUT - an isolated
checkout, since a fresh default configure may rewrite hw/bsp/family.json - and writes
DIR/<board>.json: per example, the repo files its live objects compiled or included
(ninja -t deps), its link inputs (ninja -t query), and the files CMake read to configure
the board. An object that defines no symbol (a class or port driver whose CFG_* guard
compiled its body away) contributes nothing: its source changing cannot change the
firmware. The build dir is removed afterwards; an existing DIR/<board>.json for the same
HEAD is kept unless --force.

`replay` walks first-parent commits of REF (default HEAD) and, for each, compares the
(board, example) pairs whose recorded files the commit changed against what the current
ci_select's build view selects for the same paths. A required pair the selection does
not cover is an under-selection candidate, to be confirmed by reading the source. A
commit that deletes a file, or whose changed paths the graph never saw, is listed as
needing its historical tree: the current graph cannot judge it.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
DEPS_HEADER = re.compile(r'^(\S.*): #deps \d+')
FORMAT = 1


def rel(path, root, base=None):
    """Repo-relative path of `path` (relative ones resolved against `base`, default `root`),
    or None for a file outside the checkout (toolchain headers) or a generated one."""
    p = os.path.normpath(path if os.path.isabs(path) else os.path.join(base or root, path))
    r = os.path.relpath(p, root)
    return None if r.startswith('..') or r.startswith('cmake-build') else r


def parse_ninja_deps(text):
    """{object (as ninja names it): [dependency paths]} from `ninja -t deps`."""
    out, cur = {}, None
    for line in text.splitlines():
        m = DEPS_HEADER.match(line)
        if m:
            cur = out.setdefault(m.group(1), [])
        elif line.startswith('    ') and cur is not None:
            cur.append(line.strip())
        elif not line.strip():
            cur = None
    return out


def defines_symbols(readelf_text):
    """Whether `readelf -sW` lists any FUNC or OBJECT symbol defined in a section."""
    for line in readelf_text.splitlines():
        f = line.split()
        if len(f) >= 8 and f[3] in ('FUNC', 'OBJECT') and f[6] != 'UND':
            return True
    return False


def nm_defines_symbols(nm_text):
    """Whether `gcc-nm` lists a defined code or data symbol (an LTO object's IR symbols)."""
    for line in nm_text.splitlines():
        f = line.split()
        if len(f) >= 3 and f[1] in 'TtDdBbRrCVvWw':
            return True
    return False


def is_live(obj_path, compiler, cwd):
    """Whether an object defines anything; slim LTO objects (ch583's -flto) carry only IR,
    which readelf cannot see, so those are asked through the compiler's own gcc-nm.
    Unreadable counts as live: a false 'required' is safer than a false 'dead'."""
    r = run(['readelf', '-sW', obj_path], cwd)
    if r.returncode != 0:
        return True
    if '__gnu_lto_slim' not in r.stdout:
        return defines_symbols(r.stdout)
    try:
        n = run([compiler + '-nm', obj_path], cwd)
    except OSError:
        return True
    return n.returncode != 0 or nm_defines_symbols(n.stdout)


def compiler_of(entry):
    return entry['arguments'][0] if 'arguments' in entry else entry['command'].split()[0]


def example_of(obj):
    """'role/name' of an object under the examples build tree, or None (libraries)."""
    parts = obj.split('/')
    if len(parts) > 3 and parts[2] == 'CMakeFiles' and parts[0] in ('device', 'host', 'dual', 'typec'):
        return f'{parts[0]}/{parts[1]}'
    return None


def query_inputs(query_text):
    """Input paths listed by `ninja -t query <target>` (explicit, `|` implicit, `||` order-only)."""
    out = []
    for line in query_text.splitlines():
        if line.startswith('    '):
            t = line.strip().lstrip('|').strip()
            if t:
                out.append(t)
    return out


def scope_cmake_inputs(files, examples):
    """({example: [cmake inputs]}, [shared inputs]): an input inside examples/<role>/<name>/
    reaches that example alone (none when it is not built), every other one all of them."""
    own = {ex: [] for ex in examples}
    shared = []
    for f in files:
        parts = f.split('/')
        if len(parts) > 3 and parts[0] == 'examples' and parts[1] in ('device', 'host', 'dual', 'typec'):
            ex = f'{parts[1]}/{parts[2]}'
            if ex in own:
                own[ex].append(f)
        else:
            shared.append(f)
    return own, shared


def run(cmd, cwd, **kw):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, **kw)


def extract(build_dir, root, example=None):
    """{'examples': {ex: [files]}, 'cmake_inputs': [files]} from a built tree. With
    `example`, the tree is that one example's own build (espressif: one idf.py build per
    example), so every object and every CMake input belongs to it."""
    cc = json.load(open(os.path.join(build_dir, 'compile_commands.json')))
    live = {}
    for e in cc:
        obj = os.path.relpath(e['output'] if os.path.isabs(e['output'])
                              else os.path.join(e['directory'], e['output']), build_dir)
        ex = example or example_of(obj)
        if ex is None:
            continue
        live[obj] = (ex, is_live(os.path.join(build_dir, obj), compiler_of(e), build_dir))
    deps = parse_ninja_deps(run(['ninja', '-C', build_dir, '-t', 'deps'], root).stdout)
    files = {}
    for obj, (ex, alive) in live.items():
        if alive:
            s = files.setdefault(ex, set())
            for d in deps.get(obj, []):
                f = rel(d, root, build_dir)
                if f:
                    s.add(f)
    for ex in list(files):
        name = ex.split('/')[1]
        elf = f'{name}.elf' if example else f'{ex}/{name}.elf'
        q = run(['ninja', '-C', build_dir, '-t', 'query', elf], root).stdout
        for t in query_inputs(q):
            if not t.endswith(('.obj', '.o')):
                f = rel(t, root, build_dir)
                if f and os.path.isfile(os.path.join(root, f)):
                    files[ex].add(f)
    q = run(['ninja', '-C', build_dir, '-t', 'query', 'build.ninja'], root).stdout
    cm = sorted({f for f in (rel(p, root, build_dir) for p in query_inputs(q)) if f})
    if example:
        files.setdefault(example, set()).update(cm)
        cm = []
    return {'examples': {ex: sorted(s) for ex, s in sorted(files.items())}, 'cmake_inputs': cm}


def extract_board(build_dir, root):
    """extract() over a board's build: one tree, or one per example (espressif)."""
    if os.path.isfile(os.path.join(build_dir, 'build.ninja')):
        return extract(build_dir, root)
    out = {'examples': {}, 'cmake_inputs': []}
    for role in ('device', 'host', 'dual', 'typec'):
        rd = os.path.join(build_dir, role)
        for name in sorted(os.listdir(rd)) if os.path.isdir(rd) else ():
            d = os.path.join(rd, name)
            if os.path.isfile(os.path.join(d, 'build.ninja')):
                out['examples'].update(extract(d, root, f'{role}/{name}')['examples'])
    return out if out['examples'] else None


def cmd_graph(a):
    root = os.path.abspath(a.root)
    head = run(['git', 'rev-parse', 'HEAD'], root).stdout.strip()
    boards = list(a.board or [])
    if a.boards_file:
        boards += [b.strip() for b in open(a.boards_file) if b.strip() and not b.startswith('#')]
    if not boards:
        sys.exit('graph: no boards given')
    os.makedirs(a.out, exist_ok=True)
    sys.path.insert(0, os.path.join(root, 'tools'))
    import build as tools_build  # noqa: E402  the audited checkout's own board lookup
    for board in boards:
        path = os.path.join(a.out, f'{board}.json')
        if not a.force and os.path.isfile(path):
            old = json.load(open(path))
            if old.get('head') == head and old.get('format') == FORMAT:
                print(f'{board}: cached', flush=True)
                continue
        t0 = time.monotonic()
        build_dir = os.path.join(root, 'cmake-build', f'cmake-build-{board}')
        shutil.rmtree(build_dir, ignore_errors=True)
        r = run([sys.executable, 'tools/build.py', '-b', board], root)
        rec = {'format': FORMAT, 'head': head, 'board': board, 'config': 'default',
               'family': tools_build.find_family(board)}
        g = extract_board(build_dir, root) if r.returncode == 0 else None
        if g is None:
            rec.update(status='failed', error=(r.stdout + r.stderr)[-2000:])
        else:
            rec.update(status='ok', **g)
        rec['secs'] = round(time.monotonic() - t0, 1)
        shutil.rmtree(build_dir, ignore_errors=True)
        with open(path, 'w') as fh:
            json.dump(rec, fh)
        print(f'{board}: {rec["status"]} {rec["secs"]}s '
              f'{len(rec.get("examples", {}))} examples', flush=True)


def load_index(graph_dir):
    """({file: {(board, example)}}, {board: family}, {board: status})."""
    index, family, status = {}, {}, {}
    for name in sorted(os.listdir(graph_dir)):
        if not name.endswith('.json'):
            continue
        g = json.load(open(os.path.join(graph_dir, name)))
        status[g['board']] = g['status']
        family[g['board']] = g.get('family')
        if g['status'] != 'ok':
            continue
        own, shared = scope_cmake_inputs(g['cmake_inputs'], g['examples'])
        for f in shared:
            index.setdefault(f, set()).update((g['board'], e) for e in g['examples'])
        for ex, fs in own.items():
            for f in fs:
                index.setdefault(f, set()).add((g['board'], ex))
        for ex, files in g['examples'].items():
            for f in files:
                index.setdefault(f, set()).add((g['board'], ex))
    return index, family, status


def covered(build_view, family, example):
    if build_view['full']:
        return True
    if family not in build_view['families']:
        return False
    exs = build_view['family_examples'].get(family)
    return exs is None or example in exs


def judge(files, deleted, index, family_of, build_view):
    """One commit's verdict: required pairs, the ones the build view misses, unknown paths."""
    required = set().union(*(index.get(f, set()) for f in files)) if files else set()
    missed = sorted((b, e) for b, e in required if not covered(build_view, family_of.get(b), e))
    return {'required': len(required), 'missed': missed,
            'unseen': sorted(f for f in files if f not in index), 'deleted': sorted(deleted)}


def cmd_replay(a):
    sys.path.insert(0, str(ROOT / 'tools'))
    sys.path.insert(0, str(ROOT / 'test' / 'hil'))
    import ci_select  # noqa: E402  the rules this branch ships
    index, family_of, status = load_index(a.graph)
    commits = a.commits or run(['git', 'rev-list', '--first-parent', f'--since={a.since}', a.ref],
                               ROOT).stdout.split()
    results = []
    for c in commits:
        ns = run(['git', 'diff', '--no-renames', '--name-status', f'{c}^1', c], ROOT).stdout.split('\n')
        rows = [l.split('\t', 1) for l in ns if '\t' in l]
        files = [p for _, p in rows]
        deleted = [p for s, p in rows if s.startswith('D')]
        gd = None
        if ci_select.GET_DEPS_PATH in files:
            gd = ci_select.get_deps_changed_families(
                run(['git', 'show', f'{c}^1:{ci_select.GET_DEPS_PATH}'], ROOT).stdout,
                run(['git', 'show', f'{c}:{ci_select.GET_DEPS_PATH}'], ROOT).stdout, str(ROOT))
        view = ci_select.classify_build(files, str(ROOT), gd)
        v = judge(files, deleted, index, family_of, view)
        v.update(commit=c[:9], subject=run(['git', 'log', '-1', '--format=%s', c], ROOT).stdout.strip(),
                 full=view['full'], families=len(view['families']))
        results.append(v)
    summary = {'graph_boards': {s: sum(1 for x in status.values() if x == s) for s in set(status.values())},
               'commits': len(results),
               'with_missed': sum(1 for r in results if r['missed']),
               'need_historical': sum(1 for r in results if r['deleted'])}
    with open(a.out, 'w') as fh:
        json.dump({'summary': summary, 'commits': results}, fh, indent=1)
    print(json.dumps(summary))
    for r in results:
        if r['missed']:
            print(f"{r['commit']} {r['subject'][:60]!r}: {len(r['missed'])} missed of {r['required']}"
                  f" e.g. {r['missed'][:3]}")


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest='cmd', required=True)
    g = sub.add_parser('graph')
    g.add_argument('--root', required=True)
    g.add_argument('--out', required=True)
    g.add_argument('--board', action='append')
    g.add_argument('--boards-file')
    g.add_argument('--force', action='store_true')
    r = sub.add_parser('replay')
    r.add_argument('--graph', required=True)
    r.add_argument('--out', required=True)
    r.add_argument('--ref', default='HEAD')
    w = r.add_mutually_exclusive_group(required=True)
    w.add_argument('--since')
    w.add_argument('--commits', nargs='+')
    a = p.parse_args(argv)
    return cmd_graph(a) if a.cmd == 'graph' else cmd_replay(a)


if __name__ == '__main__':
    sys.exit(main())
