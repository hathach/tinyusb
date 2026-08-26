#!/usr/bin/env python3
"""Run a membrowse size report for one build target.

Extracted from family_add_membrowse() in hw/bsp/family_support.cmake, which used
to build this up as a ~90-line bash string handed to `eval`. Two defects motivated
the extraction: `$ENV{MEMBROWSE_API_KEY}` was expanded by CMake at *configure*
time, baking the literal key into build.ninja, and the bash `echo "$MEMBROWSE_CMD"`
diagnostic printed the key in plain text. This script reads MEMBROWSE_API_KEY from
the process environment at *run* time (only when --upload is passed) and never
prints it: the logged command line redacts it as `***`.

Usage (invoked by family_add_membrowse(), not normally run by hand):
  membrowse_report.py --build-dir DIR --ninja NINJA_BIN --target TARGET --elf PATH \\
      --target-name BOARD/EXAMPLE [--upload] [--ld SCRIPT ...] [--option EXTRA]
"""
import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys

# (?:-Wl,--script=|-T\s*)SCRIPT.ld from a `ninja -t commands` link line
LD_SCRIPT_RE = re.compile(r'(?:-Wl,--script=|-T\s*)([A-Za-z0-9_./-]+\.ld)')
# `INCLUDE "foo.ld"` / `INCLUDE foo.ld` at the start of a linker script line
INCLUDE_RE = re.compile(r'^\s*INCLUDE\s+[<"]?([^">\s]+\.ld)', re.MULTILINE)
# `--defsym=SYM=VAL` or `--defsym,SYM=VAL` from a link line
DEFSYM_RE = re.compile(r'--defsym[=,](\S+)')


def resolve_includes(seed_scripts):
    """Breadth-first-resolve nested `INCLUDE` directives starting from seed_scripts.

    An include name is resolved as-is first, then relative to the directory of the
    script that included it. Dedupes and is cycle-safe. Returns scripts in the order
    first encountered.
    """
    all_scripts = []
    pending = list(seed_scripts)
    while pending:
        next_pending = []
        for script in pending:
            if script in all_scripts:
                continue
            all_scripts.append(script)
            script_dir = os.path.dirname(script)
            try:
                with open(script) as f:
                    text = f.read()
            except OSError:
                text = ''
            for inc in INCLUDE_RE.findall(text):
                resolved = None
                if os.path.isfile(inc):
                    resolved = inc
                elif os.path.isfile(os.path.join(script_dir, inc)):
                    resolved = os.path.join(script_dir, inc)
                if resolved and resolved not in all_scripts and resolved not in next_pending:
                    next_pending.append(resolved)
        pending = next_pending
    return all_scripts


def ninja_commands(ninja, build_dir, target):
    """stdout of `<ninja> -C <build_dir> -t commands <target>`.

    Returns '' on failure instead of raising: the bash this replaces piped the same
    command into grep and never checked its exit status.
    """
    r = subprocess.run([ninja, '-C', build_dir, '-t', 'commands', target],
                        capture_output=True, text=True)
    return r.stdout


def build_membrowse_cmd(args, commands_text):
    """Compose the membrowse argv (list form, no shell) for this report."""
    if args.ld is not None:
        ld_scripts = list(args.ld)
    else:
        ld_scripts = resolve_includes(LD_SCRIPT_RE.findall(commands_text))

    def_args = []
    for sym in DEFSYM_RE.findall(commands_text):
        def_args += ['--def', sym]

    map_args = []
    map_path = args.elf + '.map'
    if os.path.isfile(map_path):
        map_args = ['--map-file', map_path]

    membrowse_exe = shutil.which('membrowse') or 'membrowse'
    option_args = args.option.split()

    cmd = [membrowse_exe, 'report'] + option_args
    if os.path.isfile(args.elf):
        cmd += [args.elf, ' '.join(ld_scripts)] + def_args + map_args
    else:
        cmd += ['--identical']

    key = None
    if args.upload:
        key = os.environ.get('MEMBROWSE_API_KEY')
        if not key:
            sys.exit('error: --upload requires MEMBROWSE_API_KEY in the environment')
        cmd += ['--upload', '--github', '--target-name', args.target_name, '--api-key', key]

    return cmd, key


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                      formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--build-dir', required=True, help='CMake binary dir (ninja -C)')
    parser.add_argument('--ninja', required=True, help='ninja executable')
    parser.add_argument('--target', required=True, help='ninja target to read link commands from')
    parser.add_argument('--elf', required=True, help='path to the built ELF')
    parser.add_argument('--target-name', required=True, help='<board>/<example>, used with --upload')
    parser.add_argument('--upload', action='store_true', help='upload the report to membrowse')
    parser.add_argument('--ld', nargs='+', default=None,
                         help='override linker scripts verbatim, skipping ninja extraction '
                              '(espressif: its final link references generated scripts by bare '
                              'filename, which ninja-graph extraction cannot resolve)')
    parser.add_argument('--option', default='', help='extra membrowse report options, space-separated')
    args = parser.parse_args(argv)

    commands_text = ninja_commands(args.ninja, args.build_dir, args.target)
    cmd, key = build_membrowse_cmd(args, commands_text)

    logged = cmd if key is None else ['***' if part == key else part for part in cmd]
    # flush before the subprocess call below: stdout is block-buffered when piped, and
    # the child's own (separately-buffered, but exits first) output would otherwise
    # land ahead of this line in a captured stream.
    print(' '.join(shlex.quote(p) for p in logged), flush=True)

    return subprocess.run(cmd).returncode


if __name__ == '__main__':
    sys.exit(main())
