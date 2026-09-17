#!/usr/bin/env python3
"""Run a membrowse size report for one build target.

Reads MEMBROWSE_API_KEY from the environment at run time (only with --upload),
never at CMake configure time, and redacts it from the logged command line.

Usage (invoked by family_add_membrowse(), not normally run by hand):
  membrowse_report.py --build-dir DIR --ninja NINJA_BIN --target TARGET --elf PATH \\
      --target-name BOARD/EXAMPLE [--upload] [--ld SCRIPT ...] [--option EXTRA]
"""
import argparse
import os
import re
import shlex
import subprocess
import sys

# `--defsym=SYM=VAL` or `--defsym,SYM=VAL` from a link line
DEFSYM_RE = re.compile(r'--defsym[=,](\S+)')


def ninja_commands(ninja, build_dir, target):
    """stdout of `<ninja> -C <build_dir> -t commands <target>`.

    Raises RuntimeError on failure: an empty result would silently report against
    membrowse's default regions instead of the real linker scripts."""
    r = subprocess.run([ninja, '-C', build_dir, '-t', 'commands', target],
                        capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"'{ninja} -C {build_dir} -t commands {target}' failed "
                           f'(exit {r.returncode}):\n{r.stderr}')
    return r.stdout


def extract_ld_scripts(commands_text):
    """Deduped linker scripts referenced by `ninja -t commands` output."""
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
    """`VAR=VALUE` --defsym values referenced in `commands_text`
    (`ninja -t commands` stdout), deduped, in the order first encountered."""
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

    key = None
    if args.upload:
        cmd += ['--upload', '--github', '--target-name', args.target_name]
        # no key (fork PRs: GHA withholds secrets) -> membrowse's tokenless GHA auth
        key = os.environ.get('MEMBROWSE_API_KEY') or None
        if key:
            cmd += ['--api-key', key]

    return cmd, key


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                      formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--build-dir', required=True, help='CMake binary dir (ninja -C)')
    parser.add_argument('--ninja', required=True, help='ninja executable')
    parser.add_argument('--target', required=True, help='ninja target to read link commands from')
    parser.add_argument('--elf', required=True, help='path to the built ELF')
    parser.add_argument('--option', default='',
                        help='extra membrowse report options, space-separated '
                             '(cmake -DMEMBROWSE_OPTION=...)')
    parser.add_argument('--target-name', required=True, help='<board>/<example>, used with --upload')
    parser.add_argument('--upload', action='store_true', help='upload the report to membrowse')
    parser.add_argument('--ld', nargs='+', default=None,
                         help='override linker scripts verbatim, skipping ninja extraction '
                              '(espressif: its final link references generated scripts by bare '
                              'filename, which ninja-graph extraction cannot resolve)')
    args = parser.parse_args(argv)

    # an --identical report (no elf) must work against a never-configured build dir
    try:
        commands_text = ninja_commands(args.ninja, args.build_dir, args.target) \
            if os.path.isfile(args.elf) else ''
    except RuntimeError as e:
        sys.exit(f'error: {e}')
    cmd, key = build_membrowse_cmd(args, commands_text)

    logged = cmd if key is None else ['***' if part == key else part for part in cmd]
    # flush: piped stdout is block-buffered and the child's output would land first
    print(' '.join(shlex.quote(p) for p in logged), flush=True)

    return subprocess.run(cmd).returncode


if __name__ == '__main__':
    sys.exit(main())
