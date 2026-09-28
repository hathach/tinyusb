#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Fetch the Linux usbtest sources for one kernel version and show what a case does.

  kernel_src.py (--tag vX.Y[.Z][-rcN] | --release RELEASE) [--case N] [--cache DIR]

--release takes `uname -r` of the rig's kernel in one of two forms only:
  upstream   X.Y.Z or X.Y.0-rcN              -> vX.Y.Z, vX.Y (for X.Y.0), vX.Y-rcN
  Debian 13+ X.Y.Z+debN-<flavour>             -> vX.Y.Z, an UPSTREAM CANDIDATE: Debian may patch
                                                 the files; compare its source package to be sure
Anything else (Debian <= 12 and Ubuntu ABI names, Fedora, Ubuntu mainline, local suffixes) is
refused: pass --tag after reading the real version from /proc/version or the distro package.

Fetches drivers/usb/misc/usbtest.c and tools/usb/testusb.c from the kernel.org stable tree into
the cache (default $XDG_CACHE_HOME/tinyusb-usbtest/<tag>) and prints their paths. --case N also
prints `case N:` of usbtest_do_ioctl() with line numbers, and every completion wait in usbtest.c
with its enclosing function and whether the API takes a timeout. It does not say which waits a
case reaches or whether a hang there recovers: read the functions the case calls.

Exit 0 ok, 1 fetch or extraction failed, 2 usage error or a refused release.
"""
import argparse
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

BASE_URL = os.environ.get('KERNEL_SRC_BASE_URL',
                          'https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/plain')
FILES = ('drivers/usb/misc/usbtest.c', 'tools/usb/testusb.c')
FETCH_TIMEOUT = 60

RE_TAG = re.compile(r'v\d[\w.+-]*')   # shape only: the fetch decides whether it exists
RE_UPSTREAM = re.compile(r'(\d+)\.(\d+)\.(\d+)(-rc\d+)?')
RE_DEBIAN = re.compile(r'(\d+)\.(\d+)\.(\d+)\+deb(\d+)(-[a-z0-9]+)+')
WAIT_APIS = {'wait_for_completion': 'no timeout',
             'wait_for_completion_timeout': 'timeout',
             'wait_for_completion_interruptible': 'no timeout, interruptible',
             'wait_for_completion_killable': 'no timeout, killable',
             'usb_sg_wait': 'no timeout of its own'}


def usage(msg):
    print(f'error: {msg}', file=sys.stderr)
    sys.exit(2)


def fail(msg):
    print(f'error: {msg}', file=sys.stderr)
    sys.exit(1)


def tag_for_release(release):
    """(tag, note) for a `uname -r` in a known form, else None."""
    m = RE_UPSTREAM.fullmatch(release)
    if m:
        major, minor, patch, rc = m.groups()
        if rc and patch != '0':
            return None   # an rc is always cut from X.Y.0
        base = f'v{major}.{minor}' if patch == '0' else f'v{major}.{minor}.{patch}'
        return base + (rc or ''), 'upstream'
    m = RE_DEBIAN.fullmatch(release)
    if m and int(m.group(4)) >= 13:
        major, minor, patch = m.group(1), m.group(2), m.group(3)
        base = f'v{major}.{minor}' if patch == '0' else f'v{major}.{minor}.{patch}'
        return base, f'upstream candidate for Debian {release}: Debian may patch these files'
    return None


def fetch(tag, cache):
    out = cache / tag
    out.mkdir(parents=True, exist_ok=True)
    paths = []
    for rel in FILES:
        dest = out / Path(rel).name
        if not dest.is_file() or dest.stat().st_size == 0:
            url = f'{BASE_URL}/{rel}?h={urllib.parse.quote(tag)}'
            # git.kernel.org answers 403 to Python's default User-Agent
            req = urllib.request.Request(url, headers={'User-Agent': 'tinyusb-usbtest-kernel-src'})
            try:
                with urllib.request.urlopen(req, timeout=FETCH_TIMEOUT) as r:
                    data = r.read()
            except (urllib.error.URLError, OSError) as e:
                fail(f'cannot fetch {rel} at {tag} ({url}): {e}')
            if not data:
                fail(f'{rel} at {tag} came back empty ({url})')
            tmp = dest.with_suffix('.part')
            tmp.write_bytes(data)
            tmp.replace(dest)
        paths.append(dest)
    return paths


def function_span(lines, name):
    """(first, last) 0-based line indexes of the definition of `name`, or None."""
    starts = [i for i, l in enumerate(lines)
              if re.match(rf'(\w[\w\s\*]*\s)?\**{name}\s*\(', l) and not l.rstrip().endswith(';')]
    if len(starts) != 1:
        return None
    for j in range(starts[0], len(lines)):
        if lines[j] == '}':
            return starts[0], j
    return None


def case_block(lines, num):
    """Numbered lines of `case num:` in usbtest_do_ioctl(); SystemExit on anything ambiguous."""
    span = function_span(lines, 'usbtest_do_ioctl')
    if span is None:
        fail('usbtest_do_ioctl() not found exactly once in usbtest.c')
    first, last = span
    # the ioctl's own switch labels sit at one tab; nested switches are indented deeper
    labels = [i for i in range(first, last) if re.match(r'\tcase \d+:', lines[i])]
    hits = [i for i in labels if re.match(rf'\tcase {num}:', lines[i])]
    if len(hits) != 1:
        fail(f'case {num}: found {len(hits)} top-level labels in usbtest_do_ioctl(), expected 1')
    start = hits[0]
    # fall-through labels share the body below them: the block ends at the first label that
    # follows some body line, else at the switch's closing brace
    def is_code(j):
        text = lines[j].strip()
        return text and j not in labels and not text.startswith(('/*', '*', '//'))
    end = next((i for i in labels if i > start and any(map(is_code, range(start + 1, i)))), None)
    if end is None:
        end = next(i for i in range(start + 1, last + 1) if lines[i] == '\t}')
    return [(i + 1, lines[i]) for i in range(start, end)]


def wait_sites(lines):
    """(line, api, note, function) for every completion wait in the file."""
    # a definition header starts at column 0 and names the function right before its '(' --
    # also the kernel's split style, where the name opens its own line after the return type
    funcs = [(i, m.group(1)) for i, l in enumerate(lines)
             if l[:1].isalpha() and not l.rstrip().endswith(';')
             and (m := re.search(r'(\w+)\s*\(', l))]
    sites = []
    api_re = re.compile(r'\b(' + '|'.join(sorted(WAIT_APIS, key=len, reverse=True)) + r')\s*\(')
    for i, l in enumerate(lines):
        m = api_re.search(l)
        if not m or l.lstrip().startswith(('*', '/*', '//')):
            continue
        owner = [name for start, name in funcs if start <= i]
        sites.append((i + 1, m.group(1), WAIT_APIS[m.group(1)], owner[-1] if owner else '?'))
    return sites


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument('--tag', help='kernel.org tag, e.g. v6.12.107 or v6.13-rc3')
    g.add_argument('--release', help='`uname -r` of the kernel under test')
    p.add_argument('--case', type=int, help='print this usbtest case and the wait sites')
    p.add_argument('--cache', type=Path,
                   default=Path(os.environ.get('XDG_CACHE_HOME') or Path.home() / '.cache') / 'tinyusb-usbtest',
                   help='where fetched files are kept (default: %(default)s)')
    args = p.parse_args()

    if args.tag:
        if not RE_TAG.fullmatch(args.tag):
            usage(f'--tag {args.tag!r} is not a kernel.org tag (vX.Y, vX.Y.Z or vX.Y-rcN)')
        tag, note = args.tag, 'given tag'
    else:
        resolved = tag_for_release(args.release)
        if resolved is None:
            usage(f'--release {args.release!r} is not an upstream or Debian 13+ release name; '
                  'read the real version (/proc/version, /proc/version_signature or the '
                  'distro package) and pass --tag')
        tag, note = resolved
    if args.case is not None and not 0 <= args.case <= 99:
        usage(f'--case {args.case} is out of range')

    usbtest_c, testusb_c = fetch(tag, args.cache)
    print(f'tag {tag} ({note})')
    print(f'usbtest.c {usbtest_c}')
    print(f'testusb.c {testusb_c}')
    if args.case is None:
        return 0

    lines = usbtest_c.read_text(errors='replace').splitlines()
    print(f'\n== case {args.case} in usbtest_do_ioctl()')
    for n, text in case_block(lines, args.case):
        print(f'{n:5d}  {text}')
    print('\n== completion waits in usbtest.c (API only; read what the case calls)')
    for n, api, what, func in wait_sites(lines):
        print(f'{n:5d}  {func}: {api} ({what})')
    return 0


if __name__ == '__main__':
    sys.exit(main())
