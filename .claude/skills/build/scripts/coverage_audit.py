#!/usr/bin/env python3
"""Audit tools/ci_select.py's build selection against the real build graph. Audit only:
nothing in selection reads its output.

  coverage_audit.py graph --root CHECKOUT --out DIR (--board B... | --boards-file F) [--force]
  coverage_audit.py replay --graph DIR --out FILE.json (--since DATE | --commits SHA...) [--ref REF]

`graph` builds every example of each board with tools/build.py in CHECKOUT - an isolated
checkout, since a fresh default configure may rewrite hw/bsp/family.json - and writes
DIR/<board>.json: per example, every repo file its elf links. That is found by walking
the elf's link inputs through archives (ninja -t query): each live object's source and
recorded deps (ninja -t deps), the linker script and other files the link command names,
and the files CMake read to configure the board. An object that defines no symbol (a
class or port driver whose CFG_* guard compiled its body away) contributes nothing: its
source changing cannot change the firmware. The build goes to a private dir,
cmake-build-audit-<pid>-<board>, removed afterwards; an existing DIR/<board>.json for
the same HEAD and format is kept unless --force. Only the
examples whose elf the build wrote are recorded: a board some of whose examples failed is
'partial' with its error, one with none or an extraction error 'failed', never an empty
success.

`replay` walks first-parent commits of REF (default HEAD) and, for each, compares the
(board, example) pairs whose recorded files the commit changed against what the current
ci_select's build view selects for the same paths. A required pair the selection does
not cover is an under-selection candidate, to be confirmed by reading the source. Paths
the graph never saw are listed per commit; a commit that deletes a file needs its
historical tree, since the current graph cannot know what built the deleted file.
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
ROLES = ('device', 'host', 'dual', 'typec')
FORMAT = 3


class ExtractError(Exception):
    pass


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


def parse_query(text):
    """{target: [explicit and implicit inputs]} from `ninja -t query t...`. Order-only
    (`||`) inputs only sequence the build, and outputs point the other way."""
    out, cur, section = {}, None, None
    for line in text.splitlines():
        if not line.startswith(' ') and line.endswith(':'):
            cur, section = out.setdefault(line[:-1], []), None
        elif line.startswith('  ') and not line.startswith('    '):
            section = line.split(':', 1)[0].strip()
        elif line.startswith('    ') and section == 'input' and cur is not None:
            s = line.strip()
            if not s.startswith('||'):
                cur.append(s.lstrip('|').strip())
    return out


def defines_symbols(readelf_text):
    """Whether `readelf -sW` lists a symbol the object defines: any FUNC or OBJECT, or a
    global/weak NOTYPE (an assembly label without .type, e.g. Default_Handler)."""
    for line in readelf_text.splitlines():
        f = line.split()
        if len(f) >= 8 and f[6] != 'UND' and (
                f[3] in ('FUNC', 'OBJECT') or (f[3] == 'NOTYPE' and f[4] in ('GLOBAL', 'WEAK'))):
            return True
    return False


def nm_defines_symbols(nm_text):
    """Whether `gcc-nm` lists a defined code or data symbol (an LTO object's IR symbols)."""
    for line in nm_text.splitlines():
        f = line.split()
        if len(f) >= 3 and f[1] in 'TtDdBbRrCVvWw':
            return True
    return False


LD_INCLUDE = re.compile(r'^\s*INCLUDE\s+"?([^\s";]+)', re.M)


def linker_includes(script, search_dirs, seen=None):
    """Scripts `script` INCLUDEs, recursively, found as ld does - the current directory
    and the -L dirs - plus the including script's own directory."""
    seen = set() if seen is None else seen
    try:
        with open(script, errors='replace') as fh:
            text = fh.read()
    except OSError:
        return seen
    for name in LD_INCLUDE.findall(text):
        for d in (os.path.dirname(script), *search_dirs):
            cand = os.path.normpath(os.path.join(d, name))
            if os.path.isfile(cand):
                if cand not in seen:
                    seen.add(cand)
                    linker_includes(cand, search_dirs, seen)
                break
    return seen


def link_files(link_cmd, root, build_dir):
    """Repo files a link command names only as option values (-Wl,--script=x.ld, -T x.ld),
    and the linker scripts those INCLUDE."""
    named, search = set(), [build_dir]
    for tok in link_cmd.split():
        for part in re.split('[,=]', tok):
            if part.startswith('-L') and len(part) > 2:
                search.append(os.path.join(build_dir, part[2:]))
                continue
            p = part[2:] if part.startswith('-T') else part
            if p and not p.startswith('-'):
                a = os.path.normpath(os.path.join(build_dir, p))
                if os.path.isfile(a):
                    named.add(a)
    for s in [s for s in named if s.endswith(('.ld', '.lds', '.x'))]:
        named |= linker_includes(s, search)
    return {f for f in (rel(a, root) for a in named) if f}


def example_elves(targets_text, example=None):
    """{example: elf target} from `ninja -t targets all`; with `example`, the tree is that
    example's own build (espressif) and its elf sits at the top."""
    names = {line.split(':', 1)[0] for line in targets_text.splitlines()}
    if example:
        elf = f'{example.split("/")[1]}.elf'
        return {example: elf} if elf in names else {}
    out = {}
    for n in names:
        parts = n.split('/')
        if len(parts) == 3 and parts[0] in ROLES and parts[2] == f'{parts[1]}.elf':
            out[f'{parts[0]}/{parts[1]}'] = n
    return out


def walk_link_inputs(elf, query, known):
    """(objects, other inputs) an elf links, through archives and phony object groups;
    `query(targets)` returns parse_query()'s mapping, `known` is every build target."""
    objs, others, seen, frontier = set(), set(), {elf}, [elf]
    while frontier:
        nxt = []
        for i in range(0, len(frontier), 100):
            for ins in query(frontier[i:i + 100]).values():
                for t in ins:
                    if t.endswith(('.obj', '.o')):
                        objs.add(t)
                    elif t in known:
                        if t not in seen:
                            seen.add(t)
                            nxt.append(t)
                    else:
                        others.add(t)
        frontier = nxt
    return objs, others


def scope_cmake_inputs(files, examples):
    """({example: [cmake inputs]}, [shared inputs]): an input inside examples/<role>/<name>/
    reaches that example alone (none when it is not built), every other one all of them."""
    own = {ex: [] for ex in examples}
    shared = []
    for f in files:
        parts = f.split('/')
        if len(parts) > 3 and parts[0] == 'examples' and parts[1] in ROLES:
            ex = f'{parts[1]}/{parts[2]}'
            if ex in own:
                own[ex].append(f)
        else:
            shared.append(f)
    return own, shared


def run(cmd, cwd, **kw):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, **kw)


def run_ok(cmd, cwd):
    r = run(cmd, cwd)
    if r.returncode != 0:
        raise ExtractError(f'{" ".join(cmd[:6])}: rc {r.returncode}: {(r.stdout + r.stderr)[-1000:]}')
    return r.stdout


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


def extract(build_dir, root, example=None):
    """{'examples': {ex: [files]}, 'cmake_inputs': [files], 'nodeps': [objects]} from a
    built tree. With `example`, the tree is that one example's own build (espressif), so
    its CMake inputs are its own too. `nodeps` lists live objects ninja recorded no deps
    for: only their source is known."""
    src_of = {}
    with open(os.path.join(build_dir, 'compile_commands.json')) as fh:
        cc = json.load(fh)
    for e in cc:
        out = e['output'] if os.path.isabs(e['output']) else os.path.join(e['directory'], e['output'])
        src_of[os.path.relpath(out, build_dir)] = e
    targets = run_ok(['ninja', '-C', build_dir, '-t', 'targets', 'all'], root)
    known = {line.split(':', 1)[0] for line in targets.splitlines()}
    deps = parse_ninja_deps(run_ok(['ninja', '-C', build_dir, '-t', 'deps'], root))

    def query(ts):
        return parse_query(run_ok(['ninja', '-C', build_dir, '-t', 'query', *ts], root))

    obj_files, nodeps, gen_files = {}, set(), {}

    def generated_inputs(target):
        """Repo files a generated file (e.g. a pioasm header) is produced from."""
        if target not in gen_files:
            gen_files[target] = set()           # a cycle guard while it resolves
            s = set()
            for t in query([target]).get(target, []):
                if t in known:
                    s |= generated_inputs(t)
                else:
                    f = rel(t, root, build_dir)
                    if f and os.path.isfile(os.path.join(root, f)):
                        s.add(f)
            gen_files[target] = s
        return gen_files[target]

    def dep_files(obj):
        s = set()
        for d in deps.get(obj, []):
            f = rel(d, root, build_dir)
            if f:
                s.add(f)
                continue
            a = os.path.normpath(os.path.join(build_dir, d))
            for t in (d, os.path.relpath(a, build_dir), a):
                if t in known:
                    s |= generated_inputs(t)
                    break
        return s

    def files_of(obj):
        if obj in obj_files:
            return obj_files[obj]
        s = set()
        e = src_of.get(obj)
        if e is None:
            f = rel(obj, root, build_dir)               # a prebuilt object in the tree
            if f:
                s.add(f)
        else:
            src = e['file'] if os.path.isabs(e['file']) else os.path.join(e['directory'], e['file'])
            # assembly may define nothing readelf can name yet still place vectors or code
            if src.endswith(('.s', '.S')) or is_live(os.path.join(build_dir, obj), compiler_of(e), build_dir):
                f = rel(src, root)
                if f:
                    s.add(f)
                if obj not in deps:
                    nodeps.add(obj)
                s |= dep_files(obj)
        obj_files[obj] = s
        return s

    # only what this build produced: a failed example's link edges exist all the same
    elves = {ex: elf for ex, elf in example_elves(targets, example).items()
             if os.path.isfile(os.path.join(build_dir, elf))}
    if not elves:
        raise ExtractError('no example elf was built')
    files = {}
    for ex, elf in sorted(elves.items()):
        objs, others = walk_link_inputs(elf, query, known)
        s = set().union(*map(files_of, objs))
        s.update(f for f in (rel(t, root, build_dir) for t in others)
                 if f and os.path.isfile(os.path.join(root, f)))
        link = run_ok(['ninja', '-C', build_dir, '-t', 'commands', elf], root).strip().splitlines()
        if link:
            s |= link_files(link[-1], root, build_dir)
        files[ex] = s
    cm = sorted({f for f in (rel(p, root, build_dir) for p in query(['build.ninja']).get('build.ninja', [])) if f})
    if example:
        files[example].update(cm)
        cm = []
    return {'examples': {ex: sorted(s) for ex, s in sorted(files.items())}, 'cmake_inputs': cm,
            'nodeps': sorted(nodeps)}


def extract_board(build_dir, root):
    """extract() over a board's build: one tree, or one per example (espressif)."""
    if os.path.isfile(os.path.join(build_dir, 'build.ninja')):
        return extract(build_dir, root)
    out = {'examples': {}, 'cmake_inputs': [], 'nodeps': []}
    for role in ROLES:
        rd = os.path.join(build_dir, role)
        for name in sorted(os.listdir(rd)) if os.path.isdir(rd) else ():
            d = os.path.join(rd, name)
            if os.path.isfile(os.path.join(d, 'build.ninja')) and os.path.isfile(os.path.join(d, f'{name}.elf')):
                g = extract(d, root, f'{role}/{name}')
                out['examples'].update(g['examples'])
                out['nodeps'] += [f'{role}/{name}:{o}' for o in g['nodeps']]
    if not out['examples']:
        raise ExtractError('no build.ninja at the board build dir or any example dir')
    return out


def cmd_graph(a):
    root = os.path.abspath(a.root)
    try:
        head = run_ok(['git', 'rev-parse', 'HEAD'], root).strip()
    except ExtractError as e:
        sys.exit(f'graph: {root} has no HEAD to key the cache on: {e}')
    boards = list(a.board or [])
    if a.boards_file:
        with open(a.boards_file) as fh:
            boards += [b.strip() for b in fh if b.strip() and not b.startswith('#')]
    if not boards:
        sys.exit('graph: no boards given')
    os.makedirs(a.out, exist_ok=True)
    sys.path.insert(0, os.path.join(root, 'tools'))
    import build as tools_build  # noqa: E402  the audited checkout's own board lookup
    for board in boards:
        path = os.path.join(a.out, f'{board}.json')
        if not a.force and os.path.isfile(path):
            with open(path) as fh:
                old = json.load(fh)
            if old.get('head') == head and old.get('format') == FORMAT:
                print(f'{board}: cached', flush=True)
                continue
        t0 = time.monotonic()
        name = f'audit-{os.getpid()}-{board}'        # never the checkout's own cmake-build-<board>
        build_dir = os.path.join(root, 'cmake-build', f'cmake-build-{name}')
        shutil.rmtree(build_dir, ignore_errors=True)
        r = run([sys.executable, 'tools/build.py', '-b', board, '--build-name', name], root)
        rec = {'format': FORMAT, 'head': head, 'board': board, 'config': 'default',
               'family': tools_build.find_family(board)}
        try:
            rec.update(status='ok' if r.returncode == 0 else 'partial', **extract_board(build_dir, root))
            if r.returncode != 0:
                rec['error'] = (r.stdout + r.stderr)[-2000:]
        except (ExtractError, OSError, ValueError) as e:
            rec.update(status='failed', error=(r.stdout + r.stderr)[-2000:] if r.returncode else f'extract: {e}')
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
        with open(os.path.join(graph_dir, name)) as fh:
            g = json.load(fh)
        if g.get('format') != FORMAT:
            raise SystemExit(f'{name}: graph format {g.get("format")}, this script reads {FORMAT}: '
                             f'regenerate it with `graph --force`')
        status[g['board']] = g['status']
        family[g['board']] = g.get('family')
        if g['status'] not in ('ok', 'partial'):      # partial: the examples that built
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
    import ci_select  # noqa: E402  the rules this branch ships
    index, family_of, status = load_index(a.graph)
    commits = a.commits or run(['git', 'rev-list', '--first-parent', f'--since={a.since}', a.ref],
                               ROOT).stdout.split()
    results = []
    for c in commits:
        try:
            ns = run_ok(['git', 'diff', '--no-renames', '--name-status', f'{c}^1', c], ROOT).split('\n')
        except ExtractError as e:      # a root commit has no ^1: no diff is not a clean one
            sys.exit(f'replay: {c}: {e}')
        rows = [l.split('\t', 1) for l in ns if '\t' in l]
        files = [p for _, p in rows]
        deleted = [p for s, p in rows if s.startswith('D')]
        gd = None
        if ci_select.GET_DEPS_PATH in files:
            gd = ci_select._deps_families(
                lambda: ci_select.git_show(f'{c}^1:{ci_select.GET_DEPS_PATH}', str(ROOT)),
                lambda: ci_select.git_show(f'{c}:{ci_select.GET_DEPS_PATH}', str(ROOT)), str(ROOT))
        view = ci_select.classify_build(files, str(ROOT), gd)
        v = judge(files, deleted, index, family_of, view)
        v.update(commit=c[:9], subject=run(['git', 'log', '-1', '--format=%s', c], ROOT).stdout.strip(),
                 full=view['full'], families=len(view['families']))
        results.append(v)
    summary = {'graph_boards': {s: sum(1 for x in status.values() if x == s) for s in set(status.values())},
               'commits': len(results),
               'with_missed': sum(1 for r in results if r['missed']),
               'with_deleted': sum(1 for r in results if r['deleted']),
               'with_unseen': sum(1 for r in results if r['unseen'])}
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
