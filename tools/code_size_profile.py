#!/usr/bin/env python3
"""TinyUSB's profile for agentrc's code-size engine (its module docstring is the contract), run by
tools/code_size.py: boards and examples as tools/build.py builds them, ESP-IDF boards through
the exported ESP-IDF or CI's image, the CI-pinned boards and the master snapshots CI stores."""
import contextlib
import glob
import json
import os
import runpy
import shutil
import subprocess

import build_utils
from membrowse_cli import IDF_LD_SCRIPTS, extract_defsyms, extract_ld_scripts, link_command, report_inputs

engine = None  # the code-size engine; its main() sets it before any hook runs

NAME = 'TinyUSB'
# resolved like filters(), so a symlinked checkout still matches its filter
ROOT = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
OUT_DIR = os.path.join(ROOT, 'cmake-code-size')
BASE_BRANCH = 'master'
DEFAULT_ENGINE = 'membrowse'
BASELINE_REPO = 'hathach/tinyusb'
CI_PINNED_BOARDS = os.path.join(ROOT, '.github', 'ci-pinned-boards.json')


def filters(checkout_dir):
    """The absolute path of `checkout_dir`'s `src/`: collision-free with vendored deps
    (pico-sdk, lwip, FreeRTOS, etc.) which live at unrelated paths."""
    return [engine.filter_arg(os.path.realpath(os.path.join(checkout_dir, 'src')) + os.sep)]


def unknown_boards(boards):
    """Boards of no family in this checkout: build_utils would skip every example of one,
    so a typo must fail instead."""
    import build  # tools/build.py; its import has no side effects
    with contextlib.chdir(ROOT):  # build.py reads hw/bsp from the cwd
        return [b for b in boards if build.find_family(b) is None]


def missing_examples(examples, sha=None):
    """Examples not named role/name (build.py's -e check), or with no examples/<example>
    dir in this checkout nor in commit `sha` when given: one of either tree reaches the
    report as one-sided, a typo must fail."""
    import build  # tools/build.py; its import has no side effects
    return [e for e in examples if not build.EXAMPLE_RE.fullmatch(e)
            or not os.path.isdir(os.path.join(ROOT, 'examples', e))
            and (sha is None or engine.run(['git', '-C', ROOT, 'cat-file', '-e', f'{sha}:examples/{e}']).returncode)]


def check(command, boards, examples, sha, base_local):
    errors = []
    if unknown := unknown_boards(boards):
        errors.append(f'unknown board: {", ".join(unknown)}')
    if missing := missing_examples(examples, sha):
        errors.append(f'no such example{f" here or in {sha[:10]}" if sha else ""}: {", ".join(missing)}')
    if error := esp_without_idf(boards, examples, base_local):
        errors.append(error)
    return errors


def ci_boards():
    """Boards of .github/ci-pinned-boards.json: CI's membrowse set, which covers every
    dcd/hcd driver not waived in its `uncovered` list (drivers-coverage hook)."""
    with open(CI_PINNED_BOARDS) as f:
        return [entry['board'] for entry in json.load(f)['boards']]


def prepare_base(main_root, worktree_dir):
    """Symlink each dependency the worktree's own tools/get_deps.py lists, when fetched in
    the main checkout: the worktree lacks these untracked dirs, and a base revision can
    name paths the current manifest has renamed or dropped."""
    manifest = runpy.run_path(os.path.join(worktree_dir, 'tools', 'get_deps.py'))
    engine.link_deps(main_root, worktree_dir, manifest['deps_all'])


first_error = build_utils.first_error


ESP_IDF_IMAGE = 'espressif/idf:tinyusb'  # .github/actions/setup_toolchain/espressif's tag
ESP_IDF_MISSING = (f'ESP-IDF: source $IDF_PATH/export.sh, or docker with {ESP_IDF_IMAGE} '
                   f'(docker tag the "esp-idf" image of .github/actions/setup_toolchain/toolchain.json as {ESP_IDF_IMAGE}, '
                   f'as CI does)')


def _link_hops(path):
    """The paths a symlink resolves through, its real path last."""
    hops = []
    while os.path.islink(path):
        path = os.path.normpath(os.path.join(os.path.dirname(path), os.readlink(path)))
        hops.append(path)
    return hops


def is_espressif(board, src_dir=ROOT):
    return os.path.isdir(os.path.join(src_dir, 'hw', 'bsp', 'espressif', 'boards', board))


def _idf_image():
    return bool(shutil.which('docker')) and engine.run(['docker', 'image', 'inspect', ESP_IDF_IMAGE]).returncode == 0


def esp_without_idf(boards, examples=None, base_built=False):
    """The error for espressif `boards` no exported ESP-IDF or CI's image can build, else None.
    A board builds nothing that skips each of `examples` here, unless `base_built` and one is
    absent here, which that base may have."""
    import build  # tools/build.py; its import has no side effects

    def builds(board):
        kept, dropped = build.select_examples(board, examples, root=ROOT)
        return bool(kept) or base_built and build.NO_SUCH_EXAMPLE in dropped.values()
    esp = [b for b in boards if is_espressif(b) and (not examples or builds(b))]
    if esp and engine.WINDOWS:
        return f'{", ".join(esp)} need ESP-IDF, which code_size.py does not support on Windows'
    if esp and not (shutil.which('idf.py') or _idf_image()):
        return f'{", ".join(esp)} need {ESP_IDF_MISSING}'
    return None


def _idf_command(src_dir, build_dir, name):
    """The argv that runs idf.py on `src_dir` into `build_dir`: an exported ESP-IDF, else CI's
    image as this user in a container `name`, with both mounted at their own paths so the
    elfs' DWARF matches the filters, and each symlinked dependency's real dir at every path
    its link goes through (a base worktree's links point at the checkout's, which may point
    at the main checkout's). None when neither is available."""
    if shutil.which('idf.py'):
        return ['idf.py']
    if not _idf_image():
        return None
    # the image enables ccache; kept here it serves the next run (its default hash_dir keeps
    # one tree's DWARF paths out of the other's objects)
    cache = os.path.join(OUT_DIR, '_ccache')
    os.makedirs(cache, exist_ok=True)
    mounts = {p: os.path.realpath(p) for p in map(os.path.abspath, (src_dir, build_dir, cache))}
    for dep in runpy.run_path(os.path.join(src_dir, 'tools', 'get_deps.py'))['deps_all']:
        path = os.path.join(src_dir, dep)
        mounts.update((hop, os.path.realpath(path)) for hop in _link_hops(path))
    cmd = ['docker', 'run', '--rm', '--name', name, '--user', f'{os.getuid()}:{os.getgid()}',
           '-e', 'HOME=/tmp', '-e', f'CCACHE_DIR={os.path.abspath(cache)}']
    for target, source in sorted(mounts.items()):
        cmd += ['-v', f'{source}:{target}']
    return cmd + [ESP_IDF_IMAGE, 'idf.py']


def _build_idf(src_dir, build_dir, board, example):
    """Build each ESP-IDF example project's app, as tools/build.py does (its bootloader is
    not sized); the first failure stops."""
    import build  # tools/build.py; its import has no side effects
    examples = build.select_examples(board, None if example is None else [example], root=src_dir)[0]
    if not examples:
        return subprocess.CompletedProcess([], 1, '', f'{board} builds no example')
    container = f'tinyusb-code-size-{os.getpid()}'
    idf = _idf_command(src_dir, build_dir, container)
    if idf is None:
        return subprocess.CompletedProcess([], 1, '', f'{board} needs {ESP_IDF_MISSING}')
    for ex in examples:
        ret = None
        try:
            ret = engine.run(idf + ['-C', os.path.join(src_dir, 'examples', ex), '-B', os.path.join(build_dir, ex),
                                    '-GNinja', f'-DBOARD={board}', 'app'], timeout=600)
        finally:
            # stopping the docker CLI leaves its container building; --rm only removes an exited one
            if idf[0] == 'docker' and (ret is None or ret.returncode == 124):
                engine.run(['docker', 'rm', '-f', container])
        if ret.returncode != 0:
            break
    return ret


def build(src_dir, build_dir, board, example):
    """Configure and build examples for a board. When `example` is given, only that target is
    built (`ninja -C DIR NAME`), keeping single-example workflows fast. An espressif board
    builds each example as its own ESP-IDF project."""
    import build  # tools/build.py; its import has no side effects
    if example:
        kept, dropped = build.select_examples(board, [example], root=src_dir)
        if not kept:
            return (engine.Absent(f'{example} is not in this tree') if dropped[example] == build.NO_SUCH_EXAMPLE
                    else engine.Skipped(f'{board} does not build {example}'))
    if is_espressif(board, src_dir):
        ret = _build_idf(src_dir, build_dir, board, example)
    else:
        name = example and os.path.basename(example)
        # build.py always passes its default toolchain
        ret = engine.run(build.cmake_configure_cmd(board, build_dir, [f'-DTOOLCHAIN={build.DEFAULT_TOOLCHAIN}'],
                                                   os.path.join(src_dir, 'examples')))
        # skip.txt/only.txt only mirror the CMake family filter: the configure is the truth
        if ret.returncode == 0 and name and build.registered_examples(build_dir, [example]) == []:
            return engine.Skipped(f'{board} has no {name} target')
        if ret.returncode == 0:
            # ninja itself, not `cmake --build`: cmake does not pass a timeout's SIGTERM on
            ret = engine.run(['ninja', '-C', build_dir] + ([name] if name else []), timeout=600)
    return None if ret.returncode == 0 else ret


def elfs(build_dir, example):
    """<role>/<example>/*.elf: deeper elfs are helpers, e.g. pico-sdk's bs2_default.elf, and
    cmake's _deps/ holds fetched tools, e.g. picotool's enc_bootloader.elf."""
    # escape the dir, not the wildcards: a checkout path is a path, not a pattern
    root = glob.escape(build_dir)
    return [e for e in glob.glob(f'{root}/{example or "*/*"}/*.elf')
            if not os.path.relpath(e, build_dir).startswith('_deps' + os.sep)]


def link_inputs(elf_path):
    """(the link's build dir, membrowse's inputs: the elf, the linker scripts and defsyms of its
    link, its map)."""
    build_dir = engine.find_ninja_build_dir(elf_path)
    if build_dir is None:
        raise RuntimeError(f'no build.ninja found above {elf_path} - cannot '
                           f'determine its linker scripts')
    commands = link_command('ninja', build_dir, elf_path)
    # ESP-IDF links its generated scripts by bare name via -L
    idf_ld = [os.path.join(build_dir, p) for p in IDF_LD_SCRIPTS]
    if any(map(os.path.isfile, idf_ld)):
        if not all(map(os.path.isfile, idf_ld)):
            raise RuntimeError(f'ESP-IDF build {build_dir} lacks one of {", ".join(idf_ld)}')
        ld_scripts = idf_ld
    else:
        ld_scripts = extract_ld_scripts(commands)
        if not ld_scripts:
            raise RuntimeError(f'no linker script found in the ninja build graph '
                               f'for {elf_path}')
    # from the link's working dir, as membrowse_cli.report() does
    return build_dir, report_inputs(elf_path, ld_scripts, extract_defsyms(commands))


def linkermap_path():
    path = os.path.join(ROOT, 'tools', 'linkermap', 'linkermap.py')
    if not os.path.isfile(path):
        raise FileNotFoundError(f'{path} not found - run `python3 tools/get_deps.py tools/linkermap`')
    return path


def baseline_run(repo, sha):
    """The master push Build run whose snapshots are `sha`'s base, as code_size_ci.py picks a PR's."""
    ci = engine.load_module('code_size_ci', os.path.join(ROOT, '.github', 'scripts', 'code_size_ci.py'))
    return ci.lookup_sha(repo, sha, 'yours')


def snapshot_boards(families, boards, examples, defines=()):
    """The CI-pinned boards a build leg produced, as the Membrowse upload step picks them:
    each family's pinned boards that build one of `examples` (build.py's
    resolve_ci_boards, boards-only), plus each `-b` board that is pinned. `defines` are
    the leg's -D tokens, which can enable examples (MAX3421_HOST=1)."""
    import build  # tools/build.py; its import has no side effects
    pinned = ci_boards()
    # build.py skips a -b board that builds none of the examples, before configuring it
    picked = [b for b in boards if b in pinned and build.builds_any(b, examples, defines)]
    for family in families:
        picked += build.resolve_ci_boards(CI_PINNED_BOARDS, family, True, examples, extra_defines=defines)
    return list(dict.fromkeys(picked))


skip_example = build_utils.skip_example
