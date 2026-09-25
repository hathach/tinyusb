#!/usr/bin/env python3
"""TinyUSB front end to the membrowse CLI.

MEMBROWSE_API_KEY is read from the environment at run time (only with --upload),
never at CMake configure time, and redacted from the logged command line.
"""
import argparse
import os
import re
import shlex
import subprocess
import sys

# `--defsym=SYM=VAL` or `--defsym,SYM=VAL` from a link line
DEFSYM_RE = re.compile(r'--defsym[=,](\S+)')


def link_command(ninja, build_dir, elf):
    """The command that links `elf` (a path valid from the cwd), from
    `<ninja> -C <build_dir> -t commands -s`: only the final command, not the helper
    executables' links (pico-sdk's boot_stage2) a target also depends on.

    Raises RuntimeError on failure or no command: an empty result would silently
    report against membrowse's default regions instead of the real linker scripts."""
    rel = os.path.relpath(elf, build_dir)
    r = subprocess.run([ninja, '-C', build_dir, '-t', 'commands', '-s', rel],
                        capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"'{ninja} -C {build_dir} -t commands -s {rel}' failed "
                           f'(exit {r.returncode}):\n{r.stderr}')
    if not r.stdout.strip():
        raise RuntimeError(f'no command builds {rel!r} in the ninja build graph of '
                           f'{build_dir} - cannot determine its linker scripts')
    return r.stdout


def extract_ld_scripts(commands_text):
    """Deduped linker scripts referenced by a link command."""
    scripts = []
    for line in commands_text.splitlines():
        try:
            args = shlex.split(line)
        except ValueError:
            continue
        for i, arg in enumerate(args):
            if arg == '-T' and i + 1 < len(args):
                scripts.append(args[i + 1])
            elif arg.startswith('-T') and len(arg) > 2:
                scripts.append(arg[2:])
            elif arg.startswith('-Wl,--script='):
                scripts.append(arg.split('=', 1)[1])
    return list(dict.fromkeys(scripts))


def extract_defsyms(commands_text):
    """`VAR=VALUE` --defsym values referenced in a link command, deduped, in the
    order first encountered."""
    return list(dict.fromkeys(DEFSYM_RE.findall(commands_text)))


def build_membrowse_cmd(args, commands_text):
    """Compose the membrowse argv (list form, no shell) for this report."""
    if args.ld is not None:
        ld_scripts = list(args.ld)
    else:
        ld_scripts = extract_ld_scripts(commands_text)

    def_args = []
    for sym in extract_defsyms(commands_text):
        def_args += ['--def', sym]

    map_args = []
    map_path = args.elf + '.map'
    if os.path.isfile(map_path):
        map_args = ['--map-file', map_path]

    cmd = ['membrowse', 'report'] + shlex.split(args.option)
    if os.path.isfile(args.elf):
        if not ld_scripts:
            # same silent default-regions report as a failed ninja query
            sys.exit(f'error: no linker script found in the ninja build graph for '
                      f'{args.elf!r}; pass --ld to supply linker scripts explicitly')
        cmd += [args.elf, ' '.join(ld_scripts)] + def_args + map_args
    else:
        cmd += ['--identical']

    if args.upload:
        cmd += ['--upload', '--github', '--target-name', args.target_name]
        # no key (fork PRs: GHA withholds secrets) -> membrowse's tokenless GHA auth
        key = os.environ.get('MEMBROWSE_API_KEY')
        if key:
            cmd += ['--api-key', key]

    return cmd


def redacted(cmd, secret):
    """argv with `secret` masked, for logging. By value, not by the flag before it:
    membrowse onboard takes the key as a positional (compose())."""
    return [('***' if secret and part == secret else part) for part in cmd]


def report(args):
    # an --identical report (no elf) must work against a never-configured build dir
    link = link_command(args.ninja, args.build_dir, args.elf) if os.path.isfile(args.elf) else ''
    cmd = build_membrowse_cmd(args, link)

    # flush: piped stdout is block-buffered and the child's output would land first
    logged = redacted(cmd, os.environ.get('MEMBROWSE_API_KEY'))
    print(' '.join(shlex.quote(p) for p in logged), flush=True)

    # from the link's own working dir, so a `-L .` INCLUDE resolves (pico-sdk's
    # generated pico_flash_region.ld)
    return subprocess.run(cmd, cwd=args.build_dir if link else None).returncode


def compose(board, example, num_commits, upload, api_key, extra, family=None):
    """Return the membrowse onboard argv for one board/example backfill."""
    basename = os.path.basename(example.rstrip('/'))
    build_dir = f'examples/cmake-build-{board}'
    # `membrowse onboard` runs `git clean -fdx` before every historical build,
    # which deletes this ignored build_dir - reconfigure it fresh each time.
    python = shlex.quote(sys.executable)
    quoted_build_dir = shlex.quote(build_dir)
    if family == 'espressif':
        build = (f'idf.py -C {shlex.quote(f"examples/{example}")} -B {quoted_build_dir} '
                 f'-GNinja {shlex.quote(f"-DBOARD={board}")} build')
        elf_path = f'{build_dir}/{basename}.elf'
        shim_scripts = ('esp-idf/esp_system/ld/memory.ld',
                        'esp-idf/esp_system/ld/sections.ld')
    else:
        configure = (f'cmake -S examples -B {quoted_build_dir} '
                     f'{shlex.quote(f"-DBOARD={board}")} -G Ninja '
                     f'-DCMAKE_BUILD_TYPE=MinSizeRel')
        build = f'{configure} && cmake --build {quoted_build_dir} --target {shlex.quote(basename)}'
        elf_path = f'{build_dir}/{example}/{basename}.elf'
        shim_scripts = ()
    build_script = f'{python} tools/get_deps.py -b {shlex.quote(board)} && {build}'
    shim_path = f'{build_dir}/.membrowse-onboard.ld'
    # absolute path of this script: the historical tree may predate linker-shim
    write_shim = (f'{python} {shlex.quote(os.path.abspath(__file__))} '
                  f'linker-shim ninja '
                  f'{quoted_build_dir} {shlex.quote(elf_path)} {shlex.quote(shim_path)}')
    if shim_scripts:
        write_shim += ' ' + ' '.join(map(shlex.quote, shim_scripts))
    build_script = f'{build_script} && {write_shim}'
    cmd = ['membrowse', 'onboard']
    if '--commits' not in extra:
        cmd.append(str(num_commits))
    cmd += [
        build_script,
        elf_path,
        f'{board}/{basename}',
        api_key,
    ]
    if '--binary-search' not in extra:
        role = example.split('/', 1)[0]
        cmd += ['--build-dirs', 'src/', 'hw/', 'lib/', 'examples/build_system/',
                'examples/CMakeLists.txt', f'examples/{role}/CMakeLists.txt',
                f'examples/{example.rstrip("/")}/', 'tools/get_deps.py']
    cmd += ['--ld-scripts', shim_path]
    if not upload:
        cmd.append('--dry-run')
    cmd += extra
    return cmd


def write_linker_shim(ninja, build_dir, elf, output, *scripts):
    """Write the linker scripts and defsyms of `elf`'s link to a stable path."""
    commands = link_command(ninja, build_dir, elf)
    scripts = scripts or extract_ld_scripts(commands)
    if not scripts:
        sys.exit(f'no linker script found in the ninja build graph for {elf!r}')

    lines = []
    for sym in extract_defsyms(commands):
        name, separator, value = sym.partition('=')
        if not separator:
            sys.exit(f'invalid --defsym value in ninja build graph: {sym!r}')
        lines.append(f'{name} = {value};')
    for script in scripts:
        path = script if os.path.isabs(script) else os.path.abspath(os.path.join(build_dir, script))
        if '"' in path or not os.path.isfile(path):
            sys.exit(f'linker script not found or unsupported: {path!r}')
        lines.append(f'INCLUDE "{path}"')
    with open(output, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    return 0


def onboard(args, extra):
    """Backfill membrowse history for one CI board/example.

    Thin wrapper over `membrowse onboard` that derives every convention-bound
    argument from the board and example name, so a backfill cannot land under a
    target name that differs from what CI uploads (family_support.cmake uses
    `<board>/<cmake-target>`, i.e. the example BASENAME - not the role/name path).
    Each historical build writes a small linker shim from that commit's ninja graph,
    so linker scripts and --defsym values match the ELF being reported rather than
    the caller's current checkout.

    `membrowse onboard` checks out and `git clean -fdx`s every historical commit in
    place. This wrapper confines that to a disposable worktree; each build then runs
    that commit's `tools/get_deps.py` so dependency revisions match the ELF.

    `membrowse onboard` reports from the repo root, dropping an INCLUDE found only via -L:
    rp2040/rp2350 backfills infer FLASH from the ELF (pico-sdk's pico_flash_region.ld).
    """
    api_key = os.environ.get('MEMBROWSE_API_KEY')
    if args.upload and not api_key:
        sys.exit('MEMBROWSE_API_KEY must be set in the environment for --upload')

    repo_root = os.getcwd()
    worktree_dir = os.path.join(repo_root, 'cmake-metrics', '_onboard_worktree')

    family = 'espressif' if os.path.isdir(
        os.path.join(repo_root, 'hw', 'bsp', 'espressif', 'boards', args.board)) else None
    cmd = compose(args.board, args.example, args.num_commits, args.upload,
                  api_key or 'dry-run-placeholder', extra, family)

    print('+ ' + ' '.join(shlex.quote(p) for p in redacted(cmd, api_key)), flush=True)

    if os.path.isdir(worktree_dir):
        subprocess.run(['git', 'worktree', 'remove', '--force', worktree_dir],
                       capture_output=True)
    os.makedirs(os.path.dirname(worktree_dir), exist_ok=True)
    ret = subprocess.run(['git', 'worktree', 'add', '--detach', worktree_dir, 'HEAD'],
                         capture_output=True, text=True)
    if ret.returncode != 0:
        sys.exit(f'failed to create disposable worktree at {worktree_dir}:\n{ret.stderr}')
    try:
        return subprocess.run(cmd, cwd=worktree_dir).returncode
    finally:
        subprocess.run(['git', 'worktree', 'remove', '--force', worktree_dir],
                       capture_output=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                      formatter_class=argparse.RawDescriptionHelpFormatter)
    # linker-shim is compose()'s internal callback: no help= and off the metavar hides it
    sub = parser.add_subparsers(dest='command', required=True, metavar='{report,onboard}')

    rep = sub.add_parser('report', help='size report for one build target')
    rep.add_argument('--build-dir', required=True, help='CMake binary dir (ninja -C)')
    rep.add_argument('--ninja', required=True, help='ninja executable')
    # absolute: membrowse runs from the build dir
    rep.add_argument('--elf', required=True, type=os.path.abspath, help='path to the built ELF')
    rep.add_argument('--option', default='',
                     help='extra membrowse report options, space-separated '
                          '(cmake -DMEMBROWSE_OPTION=...)')
    rep.add_argument('--target-name', required=True, help='<board>/<example>, used with --upload')
    rep.add_argument('--upload', action='store_true', help='upload the report to membrowse')
    rep.add_argument('--ld', nargs='+', default=None,
                     help='override linker scripts verbatim, skipping ninja extraction '
                          '(espressif: its final link references generated scripts by bare '
                          'filename, which ninja-graph extraction cannot resolve)')

    onb = sub.add_parser('onboard', help='backfill membrowse history for one CI board/example',
                         description='Backfill membrowse history for one CI board/example '
                                     '(dry-run unless --upload). Extra args after -- go to '
                                     '`membrowse onboard` verbatim (see its --help). '
                                     '--upload reads MEMBROWSE_API_KEY from the environment.')
    onb.add_argument('board', help='Board name, e.g. stm32f407disco')
    onb.add_argument('example', help='Example as role/name, e.g. device/cdc_msc')
    onb.add_argument('-n', '--num-commits', type=int, default=30,
                     help='Most recent N commits to backfill (default: 30)')
    onb.add_argument('--upload', action='store_true', default=False,
                     help='Really upload (default is --dry-run)')

    shim = sub.add_parser('linker-shim')
    shim.add_argument('ninja')
    shim.add_argument('build_dir')
    shim.add_argument('elf')
    shim.add_argument('output')
    shim.add_argument('scripts', nargs='*', help='linker scripts, instead of the ninja graph\'s')

    args, extra = parser.parse_known_args(argv)
    if extra and args.command != 'onboard':
        sub.choices[args.command].error(f'unrecognized arguments: {" ".join(extra)}')
    try:
        if args.command == 'onboard':
            return onboard(args, extra)
        if args.command == 'report':
            return report(args)
        return write_linker_shim(args.ninja, args.build_dir, args.elf, args.output, *args.scripts)
    except RuntimeError as e:
        sys.exit(f'error: {e}')


if __name__ == '__main__':
    sys.exit(main())
