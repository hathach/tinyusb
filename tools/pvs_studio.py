#!/usr/bin/env python3
"""PVS-Studio over every configuration `.PVS-Studio/matrix.json` names, gated on src/.

A profile is one compile database:
- `board`: `tools/build.py -b <board> <args> --configure-only` into
  cmake-build/cmake-build-pvs-<name>, its full example set unless `args` narrows it;
- `base`: a compile-only variant of another profile's database: the src/ entries of its
  `example`, each with `-DCFG_TUSB_CONFIG_FILE="<config>"`, the named `defines` replacing the
  command's own and `-I` for each of `includes`, plus `sources` (src/ files that example does not compile)
  cloned from its src/tusb.c entry. Every entry must pass the real compiler's
  -fsyntax-only: compile-only evidence, not a firmware build.

Each database is analyzed on its own; diagnostics GA:1-3 whose position is under src/
are the findings. Coverage comes from preprocessing every src/ translation unit of every
profile with its own command: a tracked src/ file is covered when some profile keeps a
non-directive line of it; each `expect` macro must have that value in the profile's tusb.c.
`gaps` records what no profile can cover here, with a status (missing-toolchain,
missing-dependency, not-applicable) and a reason; a used gap makes the verdict
`incomplete`, an unexplained uncovered file makes it `failed`.

  pvs_studio.py list [--toolchain T] [--shard K/N] [--boards] | --ci-matrix SIZE
  pvs_studio.py run [--profile N]... [--toolchain T] [--shard K/N] [--out DIR] [--no-build]
  pvs_studio.py verdict DIR...     # aggregate the results of several runs (CI legs)

The last stdout line of run/verdict is JSON; exit 0 clean, 1 otherwise.
"""
import argparse
import concurrent.futures
import functools
import json
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MATRIX = ROOT / '.PVS-Studio' / 'matrix.json'
RULES = ROOT / '.PVS-Studio' / '.pvsconfig'
sys.path.insert(0, str(ROOT / 'tools'))
import build_utils  # noqa: E402
import family_json  # noqa: E402

LEVELS = 'GA:1,2,3'
ANALYZE_ARGS = ['--security-related-issues', '--use-old-parser', '-a', 'GA']
LINE_MARK = re.compile(r'^# \d+ "([^"]*)"')
GAP_STATUS = ('missing-toolchain', 'missing-dependency', 'not-applicable')


class Failure(Exception):
    pass


def load_matrix(path=MATRIX):
    data = json.loads(Path(path).read_text(encoding='utf-8'))
    profiles = data.get('profiles', {})
    for name, p in profiles.items():
        kinds = [k for k in ('board', 'base') if k in p]
        if len(kinds) != 1:
            raise Failure(f'profile {name}: needs exactly one of board/base')
        if 'base' in p:
            if p['base'] not in profiles or 'board' not in profiles[p['base']]:
                raise Failure(f'profile {name}: base {p["base"]} is not a board profile')
            if 'example' not in p:
                raise Failure(f'profile {name}: base profile needs an example')
    for path_, g in data.get('gaps', {}).items():
        if g.get('status') not in GAP_STATUS or not g.get('reason'):
            raise Failure(f'gap {path_}: needs status in {GAP_STATUS} and a reason')
    return data


def toolchain_of(matrix, name):
    p = matrix['profiles'][name]
    return p.get('toolchain') or matrix['profiles'].get(p.get('base'), {}).get('toolchain', 'arm-gcc')


@functools.lru_cache(maxsize=None)
def rel(path):
    """Repo-relative posix path of an absolute one, through symlinks; None outside the repo."""
    try:
        return Path(os.path.realpath(path)).relative_to(ROOT).as_posix()
    except ValueError:
        return None


def entry_path(e):
    return rel(Path(e['directory'], e['file']))


def tusb_entry(entries):
    return next((e for e in entries if entry_path(e) == 'src/tusb.c'), None)


def build_dir(name):
    return ROOT / 'cmake-build' / f'cmake-build-pvs-{name}'


def build_board(name, p):
    """Configure only, which writes the whole compile database."""
    argv = [sys.executable, str(ROOT / 'tools' / 'build.py'), '-b', p['board'], '--build-name', f'pvs-{name}',
            '--configure-only', *p.get('args', [])]
    r = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        out = r.stdout + r.stderr
        raise Failure(f'build failed: {" ".join(argv[1:])}\n{build_utils.first_error(out) or out[-2000:]}')


def board_entries(name):
    root = build_dir(name)
    dbs = [d for d in [root / 'compile_commands.json'] + sorted(root.glob('*/*/compile_commands.json')) if d.is_file()]
    if not dbs:
        raise Failure(f'no compile database under {root}')
    out = []
    for db in dbs:
        out.extend(json.loads(db.read_text(encoding='utf-8')))
    return out


def example_of(entry):
    """role/example a cmake entry's object belongs to, '' when it names none."""
    obj = entry.get('output')
    if not obj:
        tokens = family_json.compile_tokens(entry)
        obj = next((tokens[i + 1] for i, tok in enumerate(tokens[:-1]) if tok == '-o'), '')
    # older CMake writes the object path relative to `directory`
    m = re.search(r'/(device|host|dual|typec)/([^/]+)/CMakeFiles/', os.path.join(entry['directory'], obj))
    return f'{m.group(1)}/{m.group(2)}' if m else ''


def define_name(tok):
    return tok[2:].split('=', 1)[0]


def variant_entries(p, entries):
    """`base` profile entries: the example's src/ entries rewritten as the profile names."""
    src = [e for e in entries if example_of(e) == p['example'] and (entry_path(e) or '').startswith('src/')]
    if not src:
        raise Failure(f'example {p["example"]} has no src/ entries in its base database')
    defines = dict(p.get('defines', {}))
    if 'config' in p:
        defines['CFG_TUSB_CONFIG_FILE'] = '"' + str(ROOT / p['config']) + '"'
    out = []
    for e in src:
        kept, seen = [], []
        for t in family_json.compile_tokens(e):
            if t == '-D':
                raise Failure(f'{e["file"]}: split -D form not supported')
            if t.startswith('-D') and define_name(t) in defines:
                seen.append(define_name(t))
                continue
            kept.append(t)
        twice = sorted({n for n in seen if seen.count(n) > 1})
        if twice:
            raise Failure(f'{e["file"]}: {twice} defined more than once, replacement is ambiguous')
        i = kept.index('-c') if '-c' in kept else len(kept) - 1
        kept[i:i] = [f'-D{k}={v}' for k, v in defines.items()] + [f'-I{ROOT / d}' for d in p.get('includes', [])]
        out.append({'directory': e['directory'], 'file': e['file'], 'arguments': kept})
    tusb = tusb_entry(out)
    for s in p.get('sources', []):
        if tusb is None:
            raise Failure('no src/tusb.c entry to clone extra sources from')
        path = str(ROOT / s)
        args = [path if a == tusb['file'] else a for a in tusb['arguments']]
        args = [re.sub(r'tusb\.c\.obj$', Path(s).name + '.obj', a) for a in args]
        out.append({'directory': tusb['directory'], 'file': path, 'arguments': args})
    return out


def own_entries(entries):
    """The entries worth analyzing: this repository's sources outside vendored code, which an
    ESP-IDF or SDK database otherwise drowns in thousands of translation units."""
    out = []
    for e in entries:
        f = entry_path(e)
        if f and not f.startswith(('lib/', 'hw/mcu/')):
            out.append(e)
    return out


def run_tool(argv, cwd):
    try:
        return subprocess.run(argv, capture_output=True, encoding='utf-8', errors='replace', cwd=cwd)
    except OSError as ex:
        return subprocess.CompletedProcess(argv, 127, '', str(ex))


def syntax_check(entries, jobs):
    def one(e):
        r = run_tool(family_json.preprocess_argv(e) + ['-fsyntax-only', e['file']], e['directory'])
        return None if r.returncode == 0 else f'{rel(e["file"])}: {(r.stderr or r.stdout).strip()[:2000]}'
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        errs = [x for x in ex.map(one, entries) if x]
    if errs:
        raise Failure('compile check failed: ' + errs[0])


def kept_lines(stdout, directory):
    """{src file: [kept lines]} from one preprocessor output; a src/ file it includes but keeps
    nothing of maps to []."""
    out, current = {}, None
    for line in stdout.splitlines():
        m = LINE_MARK.match(line)
        if m:
            current = rel(Path(directory, m.group(1)))
            if current is not None and not current.startswith('src/'):
                current = None
            if current:
                out.setdefault(current, [])
        elif current and line.strip() and not line.lstrip().startswith('#'):
            out.setdefault(current, []).append(line)
    return out


def coverage(entries, jobs):
    """Lines kept per src/ file over a profile's src/ TUs."""
    src = [e for e in entries if (entry_path(e) or '').startswith('src/')]

    def one(e):
        r = run_tool(family_json.preprocess_argv(e) + ['-E', e['file']], e['directory'])
        if r.returncode != 0:
            return None, f'preprocess failed: {rel(e["file"])}: {r.stderr.strip()[:1000]}'
        return kept_lines(r.stdout, e['directory']), None
    counts = {}
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        for kept, err in ex.map(one, src):
            if err:
                raise Failure(err)
            for f, lines in kept.items():
                counts[f] = max(counts.get(f, 0), len(lines))
    return counts


def macro_values(entry):
    r = run_tool(family_json.preprocess_argv(entry) + ['-dM', '-E', entry['file']], entry['directory'])
    if r.returncode != 0:
        raise Failure(f'macro dump failed: {r.stderr.strip()[:1000]}')
    out = {}
    for line in r.stdout.splitlines():
        parts = line.split(None, 2)
        if len(parts) >= 2 and parts[0] == '#define':
            out[parts[1]] = parts[2] if len(parts) == 3 else ''
    return out


def check_expect(p, entries):
    exp = p.get('expect')
    if not exp:
        return
    tusb = tusb_entry(entries)
    if tusb is None:
        raise Failure('expect needs a src/tusb.c entry')
    got = macro_values(tusb)
    bad = {k: got.get(k) for k, v in exp.items() if got.get(k) != str(v)}
    if bad:
        raise Failure(f'expected macros differ: {bad}')


def platform_of(matrix, name):
    """The analyzer's data model: ILP32 for the 32-bit (and smaller) MCUs, whose `uint32_t` is an
    `unsigned long` the default LP64 model makes 64-bit, LP64 for aarch64."""
    return 'linux64' if toolchain_of(matrix, name) == 'aarch64-gcc' else 'linux32'


def analyze(db, out_log, platform):
    out_log.unlink(missing_ok=True)
    r = run_tool(['pvs-studio-analyzer', 'analyze', '-f', str(db), '-R', str(RULES), '-o', str(out_log),
                  f'-j{os.cpu_count() or 1}', '--platform', platform, *ANALYZE_ARGS], out_log.parent)
    # exit 2 with a written log is the license-expiry notice, not an analysis failure
    if r.returncode not in (0, 2) or not out_log.is_file():
        raise Failure(f'analyzer exit {r.returncode}: {(r.stdout + r.stderr).strip()[-2000:]}')


def findings(log, out_json):
    out_json.unlink(missing_ok=True)
    r = run_tool(['plog-converter', '-a', LEVELS, '-t', 'json', '-o', str(out_json), str(log)], ROOT)
    if r.returncode != 0 or not out_json.is_file():
        raise Failure(f'plog-converter exit {r.returncode}: {(r.stdout + r.stderr).strip()[-1000:]}')
    out = []
    for w in json.loads(out_json.read_text(encoding='utf-8')).get('warnings', []):
        pos = (w.get('positions') or [{}])[0]
        f = rel(pos['file']) if pos.get('file') else None
        if f and f.startswith('src/'):
            out.append({'file': f, 'line': pos.get('line'), 'code': w['code'], 'level': w['level'], 'message': w['message']})
    return out


def run_profile(matrix, name, out_dir, build, jobs):
    p = matrix['profiles'][name]
    res = {'profile': name, 'status': 'ok', 'findings': [], 'kept': {}}
    (out_dir / f'{name}.log').unlink(missing_ok=True)
    try:
        if 'board' in p:
            if build:
                build_board(name, p)
            entries = board_entries(name)
        else:
            entries = variant_entries(p, board_entries(p['base']))
            syntax_check(entries, jobs)
            check_expect(p, entries)
        db = out_dir / f'{name}.compile_commands.json'
        db.write_text(json.dumps(own_entries(entries), indent=1), encoding='utf-8')
        res['kept'] = coverage(entries, jobs)
        log = out_dir / f'{name}.log'
        analyze(db, log, platform_of(matrix, name))
        res['findings'] = findings(log, out_dir / f'{name}.json')
    except Failure as ex:
        res['status'], res['error'] = 'failed', str(ex)
    (out_dir / f'{name}.result.json').write_text(json.dumps(res, indent=1), encoding='utf-8')
    return res


def macro_only(path):
    """True when a file has nothing but comments and preprocessor directives: its code is
    the macros it defines, analyzed where a consumer expands them."""
    text = re.sub(r'/\*.*?\*/', ' ', (ROOT / path).read_text(encoding='utf-8', errors='replace'), flags=re.S)
    # the extern "C" guards are C++ only and never reach a C translation unit
    text = re.sub(r'^[ \t]*#[ \t]*ifdef[ \t]+__cplusplus[ \t]*\n[ \t]*(?:extern[ \t]+"C"[ \t]*\{|\})[ \t]*\n'
                  r'[ \t]*#[ \t]*endif\b', ' ', text, flags=re.M)
    cont = False
    for line in text.splitlines():
        line = re.sub(r'//.*', '', line).rstrip()
        directive = cont or line.lstrip().startswith('#')
        cont = directive and line.endswith('\\')
        if line.strip() and not directive:
            return False
    return True


def tracked_sources():
    r = subprocess.run(['git', 'ls-files', 'src'], cwd=ROOT, capture_output=True, text=True, check=True)
    return sorted(f for f in r.stdout.splitlines() if f.endswith(('.c', '.h')))


def verdict(matrix, results, sources=None):
    """Aggregate per-profile results into the gate's verdict."""
    sources = tracked_sources() if sources is None else sources
    expected = set(matrix['profiles'])
    have = {r['profile']: r for r in results}
    missing_profiles = sorted(expected - set(have))
    failed = {n: r.get('error', '') for n, r in have.items() if r['status'] != 'ok'}
    kept = {}
    for r in have.values():
        for f, n in r['kept'].items():
            kept[f] = max(kept.get(f, 0), n)
    gaps = matrix.get('gaps', {})

    def covered(f):
        return bool(kept.get(f)) or (f in kept and macro_only(f))
    uncovered = [f for f in sources if not covered(f)]
    used_gaps = {f: gaps[f] for f in uncovered if f in gaps}
    unexplained = [f for f in uncovered if f not in gaps]
    stale_gaps = sorted(f for f in gaps if f not in sources or covered(f))
    uniq = {}
    for r in have.values():
        for w in r['findings']:
            key = (w['file'], w['line'], w['code'], w['message'])
            uniq.setdefault(key, dict(w, profiles=[]))['profiles'].append(r['profile'])
    found = sorted(uniq.values(), key=lambda w: (w['level'], w['file'], w['line'] or 0, w['code']))
    levels = {str(lv): sum(1 for w in found if w['level'] == lv) for lv in (1, 2, 3)}
    hard = missing_profiles or failed or unexplained or stale_gaps
    # a not-applicable gap has nothing to analyze; only a missing toolchain or dependency leaves analysis undone
    missing = any(g['status'] != 'not-applicable' for g in used_gaps.values())
    status = 'failed' if hard else ('incomplete' if found or missing else 'clean')
    return {'verdict': status, 'findings': levels, 'profiles': len(have), 'missingProfiles': missing_profiles,
            'failedProfiles': failed, 'uncovered': unexplained, 'gaps': used_gaps,
            'staleGaps': stale_gaps, 'diagnostics': found}


def summary_line(v):
    keep = {k: (v[k] if k != 'failedProfiles' else {n: e.splitlines()[0][:300] for n, e in v[k].items()})
            for k in v if k != 'diagnostics'}
    return json.dumps(keep)


def print_diagnostics(v):
    for w in v['diagnostics']:
        print(f'L{w["level"]} {w["file"]}:{w["line"]} {w["code"]} {w["message"]} [{",".join(w["profiles"])}]')


def groups(matrix, toolchain=None):
    """Board profiles, each with the base profiles built on its database, in matrix order."""
    prof = matrix['profiles']
    return [[b] + [n for n, p in prof.items() if p.get('base') == b]
            for b, p in prof.items() if 'board' in p and (not toolchain or toolchain_of(matrix, b) == toolchain)]


def select(matrix, toolchain=None, shard=None, names=()):
    """Profiles to run: the named ones with the board profiles they need, else the toolchain's,
    optionally the k-th of n round-robin shards of its groups."""
    prof = matrix['profiles']
    unknown = [n for n in names if n not in prof]
    if unknown:
        raise Failure(f'unknown profile {unknown}')
    if names:
        need = set(names) | {prof[n]['base'] for n in names if 'base' in prof[n]}
        return [n for g in groups(matrix) for n in g if n in need]
    gs = groups(matrix, toolchain)
    if shard:
        k, n = (int(x) for x in shard.split('/'))
        if not 1 <= k <= n:
            raise Failure(f'bad shard {shard}')
        gs = gs[k - 1::n]
    return [n for g in gs for n in g]


def ci_matrix(matrix, size):
    out = []
    for tc in sorted({toolchain_of(matrix, n) for n in matrix['profiles']}):
        n = -(-len(groups(matrix, tc)) // size)
        out.extend({'toolchain': tc, 'shard': f'{k}/{n}'} for k in range(1, n + 1))
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    ls = sub.add_parser('list')
    ls.add_argument('--toolchain')
    ls.add_argument('--shard', help='k/n')
    ls.add_argument('--boards', action='store_true', help='print -b <board> per selected board profile')
    ls.add_argument('--ci-matrix', type=int, metavar='SIZE', help='JSON legs of at most SIZE board profiles')
    rn = sub.add_parser('run')
    rn.add_argument('--profile', action='append', default=[])
    rn.add_argument('--toolchain')
    rn.add_argument('--shard', help='k/n')
    rn.add_argument('--out', default=str(ROOT / 'cmake-build' / 'pvs-studio'))
    rn.add_argument('--no-build', action='store_true')
    rn.add_argument('-j', '--jobs', type=int, default=os.cpu_count() or 1)
    vd = sub.add_parser('verdict')
    vd.add_argument('dirs', nargs='+')
    args = ap.parse_args(argv)
    try:
        matrix = load_matrix()
        if args.cmd == 'list' and args.ci_matrix:
            print(json.dumps(ci_matrix(matrix, args.ci_matrix)))
            return 0
        names = select(matrix, args.toolchain, args.shard, args.profile if args.cmd == 'run' else ()) \
            if args.cmd != 'verdict' else []
    except (Failure, OSError, ValueError) as ex:
        print(json.dumps({'verdict': 'failed', 'error': str(ex)}))
        return 1

    if args.cmd == 'list':
        if args.boards:
            print(' '.join(f'-b {matrix["profiles"][n]["board"]}' for n in names if 'board' in matrix['profiles'][n]))
        else:
            print('\n'.join(f'{n} ({toolchain_of(matrix, n)})' for n in names))
        return 0
    if args.cmd == 'run':
        out = Path(args.out).resolve()
        out.mkdir(parents=True, exist_ok=True)
        results = []
        for n in names:
            r = run_profile(matrix, n, out, not args.no_build, args.jobs)
            print(f'{n}: {r["status"]} {len(r["findings"])} findings' + (f' - {r["error"].splitlines()[0]}' if r['status'] != 'ok' else ''), file=sys.stderr)
            results.append(r)
        (out / 'pvs-studio.sarif').unlink(missing_ok=True)
        logs = [str(out / f'{n}.log') for n in names if (out / f'{n}.log').is_file()]
        sarif_error = None
        if logs:
            # one SARIF run per invocation: code scanning rejects several runs of one tool and category
            r = run_tool(['plog-converter', '-a', LEVELS, '-t', 'sarif', '-o', str(out / 'pvs-studio.sarif'), *logs], ROOT)
            if r.returncode != 0 or not (out / 'pvs-studio.sarif').is_file():
                sarif_error = f'plog-converter exit {r.returncode}: {(r.stdout + r.stderr).strip()[-1000:]}'
        if len(names) < len(matrix['profiles']):
            # coverage spans every profile: a partial run reports its findings, the aggregate judges coverage
            v = verdict(dict(matrix, profiles={n: matrix['profiles'][n] for n in names}), results)
            v = {k: v[k] for k in ('findings', 'profiles', 'failedProfiles', 'diagnostics')}
            v['partial'] = True
            v['verdict'] = 'failed' if v['failedProfiles'] else ('incomplete' if v['diagnostics'] else 'clean')
        else:
            v = verdict(matrix, results)
        if sarif_error:
            v['verdict'], v['sarifError'] = 'failed', sarif_error
    else:
        results = []
        for d in args.dirs:
            results.extend(json.loads(p.read_text(encoding='utf-8')) for p in sorted(Path(d).glob('*.result.json')))
        v = verdict(matrix, results)
    print_diagnostics(v)
    print(summary_line(v))
    return 0 if v['verdict'] == 'clean' else 1


if __name__ == '__main__':
    sys.exit(main())
