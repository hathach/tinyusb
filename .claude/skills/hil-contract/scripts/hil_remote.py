#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run hil_test.py on a remote rig (default ci.lan) from a dev PC.

Takes hil_test.py's own arguments, minus the config: stages the harness, the config and the
firmware the run will look for under -B (default cmake-build, the build skill's --shared
layout), runs it there, and copies the report pair, its attempt records and the
<config>.failed re-run spec back to the checkout root.

The transport -- the leased, wiped tree, the guarded staging, run and fetch, the run records
-- is agentrc's hil_remote_core, from this script's checkout's lib/agentrc at the pin; the
report document is helper/hil_report.py over agentrc's hil_report_core.

`--run-id ID` records the run under <checkout>/.hil-remote/: ID.started.json, locked for as
long as the run lives, then the done record ID.done.json (exit status, the report files it
copied back). `wait ID` blocks until then, at most --timeout seconds, and prints one JSON
status line.

`receipt --out FILE --head SHA [-b BOARD]...` writes a build receipt: HEAD, the roster's digest
and the digest of every file a run of those boards would stage, refusing an unbuilt variant, a
stale example's firmware left in a variant dir, a tree that is not clean or a HEAD other than
SHA, the one the build began at; the build skill's check_build.py --receipt runs it right after
its build.
`--receipt FILE` on a run refuses to stage unless HEAD, the roster and every staged file still
match it, stages the snapshot of the firmware it checked, and runs nothing when HEAD, the roster
or a tracked file moved while it staged.

Env overrides: REMOTE (ssh host), REMOTE_DIR (rm -rf'd and recreated each run, under a lock
on <REMOTE_DIR>.lock that refuses a second run sharing it), CONFIG (HIL config json), ROOT_DIR
(checkout to test; defaults to this script's own).
"""
import argparse
import importlib
import importlib.util
import json
import os
import re
import shlex
import shutil
import sys
import tempfile
from pathlib import Path


def _load_core():
    """hil_remote_core through this checkout's helper/agentrc.py, by path: the adapter and its
    transport come from one checkout whatever ROOT_DIR names."""
    spec = importlib.util.spec_from_file_location(
        'tinyusb_hil_remote_agentrc', Path(__file__).resolve().parents[4] / 'test/hil/helper/agentrc.py')
    agentrc = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(agentrc)
    return agentrc.load('skills/hil/scripts/hil_remote_core.py', 'tinyusb_hil_remote_core')


try:
    core = _load_core()
except ImportError as e:   # lib/agentrc off its pin: the message names the fetch
    sys.exit(f'error: {e}')
fail = core.fail

ROOT = Path(os.environ.get('ROOT_DIR') or Path(__file__).resolve().parents[4]).resolve()

# Everything the rig executes, by repo-relative path. test_hil_bounded's RemoteStaging walks
# the harness's import closure and requires each file here, so keep this a plain literal.
# test/hil/pyocd/ is pyocd's project dir, which the provider requires to exist: its README keeps it staged.
# helper/agentrc.py exec_module's agentrc's shared scripts (the board-lock primitive, the USB
# observation, the RTT console, the flash provider, the TT reset, the usbtest mechanics) after
# checking lib/agentrc/.git/HEAD against the commit tools/get_deps.py pins: the lib/agentrc and
# tools/ entries let the rig's tree pass the same check, and RemoteStaging requires one per
# agentrc.load target and per file a target loads or launches by path (the provider's runner,
# pyocd launcher and user script, the openocd_esp backend's esp_session, and the TT reset's
# root child, which is hil_tt.py itself). helper/hil_report.py loads the report core.
HARNESS_FILES = (
    'test/hil/hil_test.py',
    'test/hil/hil_flash.py',
    'test/hil/usbtest.py',
    'test/hil/pymtp.py',
    'test/hil/mtp_test.py',
    'test/hil/mtp_raw.py',
    'test/hil/helper/__init__.py',
    'test/hil/helper/agentrc.py',
    'test/hil/helper/hil_args.py',
    'test/hil/helper/hil_lock.py',
    'test/hil/helper/hil_report.py',
    'test/hil/helper/hil_tt.py',
    'test/hil/helper/hil_util.py',
    'test/hil/helper/usb_observe.py',
    'test/hil/pyocd/README.md',
    'tools/get_deps.py',
    'lib/agentrc/.git/HEAD',
    'lib/agentrc/skills/hil/scripts/hil_flash.py',
    'lib/agentrc/skills/hil/scripts/hil_openocd_esp.py',
    'lib/agentrc/skills/hil/scripts/hil_process.py',
    'lib/agentrc/skills/hil/scripts/pyocd/run_pyocd.py',
    'lib/agentrc/skills/hil/scripts/pyocd/user_script.py',
    'lib/agentrc/skills/hil/scripts/hil_lock.py',
    'lib/agentrc/skills/hil/scripts/hil_report_core.py',
    'lib/agentrc/skills/hil/scripts/hil_tt.py',
    'lib/agentrc/skills/hil/scripts/hil_usbtest.py',
    'lib/agentrc/skills/hil/scripts/usb_observe.py',
    'lib/agentrc/skills/rtt/scripts/rtt.py',
    'lib/agentrc/skills/target-debug/scripts/esp_session.py',
)

STAGED_SUFFIXES = ('.elf', '.bin', '.hex')
STAGED_NAMES = ('config.env', 'flash_args')
FIRMWARE_FILTER = ['--prune-empty-dirs', '--include=*/', *(f'--include=*{x}' for x in STAGED_SUFFIXES),
                   *(f'--include={n}' for n in STAGED_NAMES), '--exclude=*']
# a variant dir's <role>/<name> example dirs, examples/'s layout, which hil_test.py flashes from
EXAMPLE_ROLES = ('device', 'dual', 'host', 'typec')

RUNS_DIR = '.hil-remote'

def helper(name):
    """A test/hil/helper module, imported from this checkout's harness."""
    if str(ROOT / 'test' / 'hil') not in sys.path:
        sys.path.insert(0, str(ROOT / 'test' / 'hil'))
    return importlib.import_module(f'helper.{name}')


def build_command(config_path, boards, build_dir):
    """The build contract's line for the dirs hil_test.py flashes, each roster variant included."""
    if config_path.is_relative_to(ROOT):
        config_path = config_path.relative_to(ROOT)
    board_args = ' '.join(f'--board {shlex.quote(b)}' for b in boards) or '--board <board>'
    line = (f'python3 .claude/skills/build/scripts/check_build.py {board_args} --shared '
            f'--variants {shlex.quote(config_path.as_posix())}')
    if build_dir != 'cmake-build':
        line += f'\n  (it writes under cmake-build/, not {build_dir}/)'
    return line


def select_boards(config, args):
    """The boards hil_test.py will run, refusing an unknown one: it fails the whole remote run
    after staging. A build of a board this drops satisfies nothing."""
    boards = args.board
    roster = {b['name']: b for b in config.get('boards', [])}
    unknown = [b for b in boards if b not in roster]
    if unknown:
        fail(f'not in the config: {" ".join(unknown)} (-b takes board names, not variant names)')
    selected = boards or list(roster)
    if args.flasher or args.exclude_flasher:
        selected = [n for n in selected if roster[n]['flasher']['name'] not in args.exclude_flasher
                    and (not args.flasher or roster[n]['flasher']['name'] in args.flasher)]
        if not selected:
            fail(f'no board left after the flasher filter (--flasher {args.flasher or "-"}, '
                 f'--exclude-flasher {args.exclude_flasher or "-"})')
    return selected


def variant_dirs(config, build_dir, board):
    """The <build_dir>/cmake-build-<variant> dirs hil_test.py flashes a board's variants from."""
    return [ROOT / build_dir / f'cmake-build-{v}' for v in helper('hil_report').variants_of(config, board)]


def resolve_firmware(config, config_path, args):
    """Return the existing <build_dir>/cmake-build-<variant> dirs to stage, refusing before
    anything remote is touched: a board with no build at all would only show up as
    `Skip (no binary)` rows."""
    boards, build_dir = args.board, args.build_dir
    selected = select_boards(config, args)
    dirs, missing, unbuilt = [], [], []
    for name in selected:
        found = variant_dirs(config, build_dir, name)
        present = [d for d in found if d.is_dir()]
        if boards and not present:
            missing.append(f'  {name}: none of {", ".join(d.relative_to(ROOT).as_posix() for d in found)}')
            unbuilt.append(name)
        for i, d in enumerate(found):
            if d not in present and boards:
                # hil_test.py logs `Skip (no binary)` and exits 0 for these cells, except that a
                # one-test run that flashes fails a later variant's same-PID boundary (HIL contract, Build)
                boundary = '' if args.skip_flash or i == 0 else ' or fail the same-PID boundary on a one-test run'
                print(f'warning: {name}: no {d.relative_to(ROOT).as_posix()} -- its cells will be skipped{boundary}',
                      file=sys.stderr)
        dirs += present
    if missing:
        fail(f'no build under {build_dir}/ for:\n' + '\n'.join(missing) +
             f'\nbuild with\n  {build_command(config_path, unbuilt, build_dir)}')
    if not dirs:
        fail(f'no {build_dir}/cmake-build-* build for any selected board in the config -- nothing to test; '
             f'build with\n  {build_command(config_path, selected, build_dir)}')
    return dirs


def sidecar_matches(sidecar, config):
    """hil_report.json is not namespaced by rig, so a sidecar from another rig or config
    would merge boards that never ran here. Require one row from this roster."""
    try:
        rows = json.loads(sidecar.read_text(encoding='utf-8')).get('rows') or []
    except (OSError, ValueError, AttributeError):
        return False
    variants_of = helper('hil_report').variants_of
    known = {n for b in config.get('boards', []) for n in (b['name'], *variants_of(config, b['name']))}
    return not rows or any(isinstance(r, dict) and r.get('board') in known for r in rows)


def forwarded_env():
    """HIL_* knobs for the run. HIL_REPORT_DIR stays local: the copy-back reads the report
    from the remote dir, and relocating it would bring nothing home."""
    return {k: v for k, v in sorted(os.environ.items())
            if re.fullmatch(r'HIL_[A-Z0-9_]*', k) and k != 'HIL_REPORT_DIR'}


def harness_argv(argv, config_name):
    """hil_test.py's command line on the rig. --retry 1 first so a caller's -r wins by
    argparse's last-wins; the pool guard does not scale with retries, so a higher default
    would starve a shared rig."""
    return ['python3', '-u', 'test/hil/hil_test.py', '--retry', '1', *argv, f'test/hil/{config_name}']


def staged_paths(dirs):
    """The files FIRMWARE_FILTER stages from dirs."""
    return [f for d in dirs for f in sorted(d.rglob('*'))
            if f.is_file() and (f.suffix in STAGED_SUFFIXES or f.name in STAGED_NAMES)]


def staged_files(dirs, base=None):
    """{path relative to base (default the checkout): sha256} of what FIRMWARE_FILTER stages from dirs."""
    return core.file_digests(base or ROOT, staged_paths(dirs))


def snapshot(dirs, into):
    """Copy what FIRMWARE_FILTER stages from dirs to the same checkout-relative layout under
    into (hil_remote_core.snapshot), returning the copied dirs to stage from."""
    core.snapshot(ROOT, staged_paths(dirs), into)
    copies = [into / d.relative_to(ROOT) for d in dirs]
    for c in copies:
        c.mkdir(parents=True, exist_ok=True)   # rsync needs every source, staged files or not
    return copies


def example_firmware(d, files):
    """The <role>/<name> example dirs of variant dir d holding staged <name>.<suffix> firmware,
    directly or in one config subdir, where hil_flash.find_firmware() looks."""
    prefix = f'{d.relative_to(ROOT).as_posix()}/'
    return {'/'.join(parts[:2]) for parts in (p[len(prefix):].split('/') for p in files if p.startswith(prefix))
            if len(parts) in (3, 4) and parts[0] in EXAMPLE_ROLES
            and Path(parts[-1]).stem == parts[1] and Path(parts[-1]).suffix in STAGED_SUFFIXES}


def unverified_examples(board, d, examples):
    """The examples of variant dir d the board's build does not build: a shared dir keeps a
    skipped or dropped example's old firmware. The same test as check_build's configured()."""
    if str(ROOT / 'tools') not in sys.path:
        sys.path.insert(0, str(ROOT / 'tools'))
    tools_build = importlib.import_module('build')
    if [p.parent.parent.name for p in ROOT.glob(f'hw/bsp/*/boards/{board}')] == ['espressif']:
        # one idf tree per example: the build attempted the ones tools/build.py does not skip
        cwd = os.getcwd()
        os.chdir(ROOT)  # tools/build.py reads examples/ and hw/bsp/ relative to the working directory
        try:
            built = {e for e in tools_build.get_examples('espressif') if not tools_build.build_utils.skip_example(e, board)}
        finally:
            os.chdir(cwd)
    else:
        registered = tools_build.cmake_registered_targets(str(d))
        if registered is None:
            fail(f'cmake cannot list the targets of {d.relative_to(ROOT).as_posix()}, so the firmware its build made cannot be told '
                 f'from a stale one')
        built = {e for e in examples if e.split('/')[1] in registered}
    return sorted(examples - built)


def parse_run(argv):
    """(args, config, config path) of hil_test.py's arguments."""
    config_path = Path(os.environ.get('CONFIG') or ROOT / 'test/hil/tinyusb.json').resolve()
    if not (ROOT / 'test/hil/hil_test.py').is_file():
        fail(f'{ROOT} does not look like a tinyusb checkout')
    args = helper('hil_args').build_parser().parse_args([*argv, str(config_path)])
    core.check_relative(args.build_dir, '-B')
    try:
        config = json.loads(config_path.read_text(encoding='utf-8'))
    except (OSError, ValueError) as e:
        fail(f'could not read the config {config_path}: {e}')
    return args, config, config_path


def write_receipt(argv):
    out, argv = core.pop_option(argv, '--out')
    if not out:
        fail('receipt needs --out FILE')
    built_head, argv = core.pop_option(argv, '--head')
    if not built_head:
        fail('receipt needs --head SHA, the HEAD the build began at')
    args, config, config_path = parse_run(argv)
    boards = select_boards(config, args)
    dirs = [(b, d) for b in boards for d in variant_dirs(config, args.build_dir, b)]
    files = staged_files([d for _, d in dirs])
    # only example firmware: CMake's compiler-ABI probes leave a .bin in a dir never built
    examples = {d: example_firmware(d, files) for _, d in dirs}
    unbuilt = [d.relative_to(ROOT).as_posix() for _, d in dirs if not examples[d]]
    if unbuilt:
        fail(f'not built: {", ".join(unbuilt)}; a receipt covers every variant the run selects, build with\n'
             f'  {build_command(config_path, boards, args.build_dir)}')
    stale = [f'{d.relative_to(ROOT).as_posix()}/{e}' for b, d in dirs for e in unverified_examples(b, d, examples[d])]
    if stale:
        fail(f'firmware of an example the build does not build: {", ".join(stale)}; a receipt pins only firmware '
             f'the build verified and a run stages every file there, so remove those dirs and write the receipt again')
    dirty = core.git(ROOT, 'status', '--porcelain', '--untracked-files=normal')
    if dirty:
        fail(f'the tree is not clean:\n{dirty}\na receipt pins the firmware to a commit: commit these (a hw/bsp/family.json '
             f'a build rewrote included) or remove them, then rebuild on that commit')
    head = core.git(ROOT, 'rev-parse', 'HEAD')
    if head != built_head:
        fail(f'HEAD is {head[:12]}, not {built_head[:12]} the build began at: a commit landed during the build, '
             f'so its firmware is not of HEAD; build again')
    receipt = {'head': head, 'configDigest': core.digest(config_path), 'boards': boards, 'files': files}
    Path(out).parent.mkdir(parents=True, exist_ok=True)
    core.write_json(Path(out), receipt)
    print(json.dumps({**receipt, 'files': len(receipt['files'])}))
    return 0


def check_receipt(path, config_path, boards, dirs, staged):
    """Refuse to stage firmware the build receipt at path does not pin to this HEAD and roster:
    dirs are every variant dir of the run's boards, staged the checkout-relative digests of
    the firmware to stage."""
    try:
        receipt = json.loads(Path(path).read_text(encoding='utf-8'))
        head, pinned = receipt['head'], receipt['files']
        config_digest, built = receipt['configDigest'], receipt['boards']
    except (OSError, ValueError, KeyError, TypeError) as e:
        fail(f'unusable build receipt {path}: {e!r}')
    unbuilt = [b for b in boards if b not in built]
    differ = core.differing(pinned, staged, [f'{d.relative_to(ROOT).as_posix()}/' for d in dirs])
    why = [*checkout_drift((head, config_digest), config_path),
           unbuilt and f'it does not cover {", ".join(unbuilt)}',
           differ and f'{len(differ)} staged file(s) differ from it, first {", ".join(differ[:5])}']
    if any(why):
        fail(f'the build receipt {path} does not match: {"; ".join(filter(None, why))}; rebuild and write a new receipt')
    return head, config_digest


def checkout_drift(pinned, config_path):
    """Why the harness and roster the checkout stages are not those of pinned (HEAD, roster digest):
    they are HEAD's only on a clean tree."""
    head, config_digest = pinned
    return core.checkout_drift(ROOT, head, {config_path: config_digest})


def check_local_report():
    """Refuse, before the rig is touched, a local report the copy-back could not archive
    before replacing it: one of an unknown schema or version."""
    report = helper('hil_report')
    reading = report.core().read_document(ROOT / report.REPORT_JSON, report.schema())
    if reading.outcome == 'unsupported':
        fail(f'{ROOT / report.REPORT_JSON}: {reading.detail}; the copy-back would replace it unarchived, '
             f'so move it away first')


def run(argv, receipt=None):
    """One remote run: (its exit status, the report files copied back)."""
    remote = os.environ.get('REMOTE', 'ci.lan')
    remote_dir = os.environ.get('REMOTE_DIR', '/tmp/tinyusb-hil')
    core.check_remote_dir(remote_dir)
    args, config, config_path = parse_run(argv)
    try:
        helper('hil_lock')   # the rig refuses an off-pin lib/agentrc too, but only after the wipe
    except ImportError as e:
        if e.name != 'fcntl':   # Windows: agentrc.load() passed the pin check before the POSIX-only lock ran
            fail(e)
    if args.build:
        fail(f'--build would build on the rig, which gets binaries only; build locally with\n'
             f'  {build_command(config_path, select_boards(config, args), args.build_dir)}')
    firmware = resolve_firmware(config, config_path, args)
    snap_parent = None
    if os.name == 'nt':   # in the checkout, where local_operand() can name it to rsync
        snap_parent = ROOT / RUNS_DIR
        snap_parent.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='hil-remote-', dir=snap_parent) as snap:
        pinned = None
        if receipt is not None:
            firmware = snapshot(firmware, Path(snap))
            boards = select_boards(config, args)
            pinned = check_receipt(receipt, config_path, boards,
                                   [d for b in boards for d in variant_dirs(config, args.build_dir, b)],
                                   staged_files(firmware, Path(snap)))
        for p in (config_path, *firmware):
            core.local_operand(ROOT, p)   # Windows: refuse one rsync cannot name now, not after the lease wiped the rig's tree
        core.check_relative(f'{config_path.name}.failed', 'the re-run spec the copy-back fetches')
        try:
            check_local_report()
        except ImportError as e:
            fail(e)

        print(f'==> Setting up remote {remote}:{remote_dir}')
        with core.remote_lease(remote, remote_dir, ROOT, ('test/hil/helper', 'tools', args.build_dir)) as tree:
            return stage_and_run(tree, argv, args, config, config_path, firmware, receipt, pinned)


def stage_and_run(tree, argv, args, config, config_path, firmware, receipt=None, pinned=None):
    upload = []
    if args.accumulate:
        report = helper('hil_report')
        # the wipe cleared the rig's copy; without the local sidecar as merge base the retry's
        # small table replaces the full one
        sidecar = ROOT / report.REPORT_JSON
        if not sidecar.is_file():
            print('==> warning: --accumulate but no local hil_report.json to merge onto -- this '
                  "run's table will REPLACE the previous one", file=sys.stderr)
        elif not sidecar_matches(sidecar, config):
            print(f'==> warning: hil_report.json holds no board from {config_path.name} -- not '
                  "uploading it; this run's table will REPLACE the previous one", file=sys.stderr)
        else:
            print('==> Uploading hil_report.json as the --accumulate merge base')
            upload = [report.REPORT_JSON, *chain_records(report)]

    print('==> Copying the harness and config')
    core.stage_files(tree, [*HARNESS_FILES, *upload])
    if core.rsync(tree, '-q', config_path, f'{tree.spec("test/hil")}/', staging=True) != 0:
        fail('could not stage the config')
    print(f'==> Copying firmware from {len(firmware)} build dir(s) under {args.build_dir}/')
    # one transfer: every dir lands under the same <build_dir>/, by its own name
    if core.rsync(tree, '-a', *FIRMWARE_FILTER, *firmware, f'{tree.spec(args.build_dir)}/', staging=True) != 0:
        fail('could not stage the firmware')
    # the harness and roster went from the live checkout, which may have moved since the receipt check
    drift = pinned and checkout_drift(pinned, config_path)
    if drift:
        fail(f'the checkout changed during the rig set-up, so what was staged is not what it checked against the build '
             f'receipt {receipt}: {"; ".join(drift)}; not running it')

    print(f'==> Running HIL test on {tree.host}')
    rc = core.run_remote(tree, harness_argv(argv, config_path.name), forwarded_env())
    if rc == core.TREE_REPLACED:
        return rc, []     # nothing of ours ran there: no report to fetch, and another run's must not land here
    copied, came_home = copy_back(tree, config_path.name)
    # the rig's exit says nothing of a report that did not come home: never 0 without one
    return rc or int(not came_home), copied


def copy_back(tree, config_name):
    """Bring the run's report home in one fetch into a scratch dir under RUNS_DIR: the pair
    all or nothing (the markdown is a rendering of the sidecar, and half a pair publishes
    last run's table beside this run's data), the rig's attempt records and <config>.failed.
    The core's adopt() then makes the pair the checkout's report or withdraws the local one
    (hil_report_core.adopt says when); the scratch dir is kept whenever something did not
    come home. Returns (the checkout-relative paths it wrote, whether this run's report came
    home readable)."""
    report = helper('hil_report')
    rcore, schema = report.core(), report.schema()
    pair = [report.REPORT_MD, report.REPORT_JSON]
    # A green run writes no .failed, so a stale local spec would re-flash boards that passed:
    # it is removed when the rig has none.
    spec = f'{config_name}.failed'
    runs = ROOT / RUNS_DIR
    runs.mkdir(exist_ok=True)
    scratch = Path(tempfile.mkdtemp(prefix='collect-', dir=runs))
    fetched = core.copy_back(tree, scratch, [pair], [spec, f'{report.REPORT_ATTEMPTS}/'])
    _, incomplete = fetched
    problems = [f'{m} {o}' for m, o in fetched.outcomes.items() if o not in ('ok', 'absent')]
    copied = []
    if fetched.outcomes.get(spec) == 'ok':
        os.replace(scratch / spec, ROOT / spec)
        copied.append(spec)
        print(f'==> {spec} copied to {ROOT / spec}')
    else:
        (ROOT / spec).unlink(missing_ok=True)
    unusable = 'the rig report did not all arrive: ' + ', '.join(f'{m} {fetched.outcomes.get(m)}' for m in pair) \
        if incomplete else None
    try:
        adopted = rcore.adopt(ROOT, scratch, schema, unusable=unusable, reason='report copy-back failed')
    except rcore.ReportError as e:   # another writer holds the checkout's report
        adopted = rcore.Adoption([], [], [str(e)], [], None)
    if adopted.conflicts:
        print(f'==> warning: attempt record(s) {", ".join(adopted.conflicts)} on the rig differ from those in '
              f'{ROOT / report.REPORT_ATTEMPTS}; the local ones are kept', file=sys.stderr)
    if report.REPORT_JSON in adopted.copied:
        print(f'==> Report copied to {ROOT / report.REPORT_MD} (+ sidecar)')
    copied += adopted.copied
    problems += adopted.problems
    for p in problems:
        print(f'==> error: report copy-back: {p}', file=sys.stderr)
    if problems or adopted.conflicts:
        print(f'==> the rig\'s report and attempt records as fetched are kept in {scratch}', file=sys.stderr)
    else:
        shutil.rmtree(scratch, ignore_errors=True)
    return copied, not problems


def chain_records(report):
    """The checkout-relative files of the attempt records the sidecar names, transitively: its
    predecessors, so the rig's report links one whose record it holds, and the reports a
    withdrawal archived, which the rig's merge reads its rows from. The rig's start_attempt
    archives the sidecar itself."""
    rcore, schema = report.core(), report.schema()
    root = ROOT / report.REPORT_ATTEMPTS
    seen, todo = set(), links(rcore.read_document(ROOT / report.REPORT_JSON, schema))
    while todo:
        aid = todo.pop()
        if not aid or aid in seen or len(seen) >= rcore.CHAIN_LIMIT:
            continue
        seen.add(aid)
        todo += links(rcore.read_document(root / aid / report.REPORT_JSON, schema))
    return [f.relative_to(ROOT).as_posix() for aid in sorted(seen)
            if (root / aid).is_dir() and not (root / aid).is_symlink()
            for f in sorted((root / aid).iterdir()) if f.is_file() and not f.is_symlink()]


def links(reading):
    """The archived attempts a report names: its predecessor, or the report a withdrawal archived."""
    if reading.meta:
        return [reading.meta['previous_attempt']]
    return [reading.body['archived']] if reading.outcome == 'withdrawn' else []


def wait(argv):
    p = argparse.ArgumentParser(prog='hil_remote.py wait',
                                description=f'exit 0 done, {core.RUNNING} still running at --timeout, '
                                            f'{core.DEAD} ended without a done record')
    p.add_argument('run_id')
    p.add_argument('--timeout', type=core.wait_secs, default=core.WAIT_SECS,
                   help=f'seconds, at most {core.WAIT_SECS} (the default)')
    a = p.parse_args(argv)
    rc, status = core.wait_local(ROOT / RUNS_DIR, a.run_id, a.timeout)
    print(json.dumps(status))
    return rc


def main(argv):
    if argv[:1] == ['wait']:
        return wait(argv[1:])
    if argv[:1] == ['receipt']:
        return write_receipt(argv[1:])
    run_id, argv = core.pop_option(argv, '--run-id')
    receipt, argv = core.pop_option(argv, '--receipt')
    if run_id is None:
        return run(argv, receipt)[0]
    return core.run_recorded(ROOT / RUNS_DIR, run_id, lambda: run(argv, receipt))


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
