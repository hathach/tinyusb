#!/usr/bin/env python3
"""Build the TinyUSB Sphinx documentation locally.

Thin wrapper around `sphinx-build` so a manual doc build is one command.
`conf.py` auto-collects example READMEs, so no extra steps are needed.

    python3 tools/build_doc.py            # build docs/_build/
    python3 tools/build_doc.py -c -W -o   # clean, fail on warnings, open result
"""
import argparse
import shutil
import subprocess
import sys
import webbrowser
from pathlib import Path

TOP = Path(__file__).parent.parent.resolve()
DOCS = TOP / "docs"
BUILD = DOCS / "_build"


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("-c", "--clean", action="store_true", help="remove docs/_build first")
    p.add_argument("-W", "--strict", action="store_true", help="treat warnings as errors")
    p.add_argument("-o", "--open", action="store_true", help="open the built docs in a browser")
    args = p.parse_args()

    if args.clean and BUILD.exists():
        shutil.rmtree(BUILD)

    cmd = ["sphinx-build", "-b", "html"]
    if args.strict:
        cmd.append("-W")
    cmd += [str(DOCS), str(BUILD)]

    print("+", " ".join(cmd))
    rc = subprocess.call(cmd)
    if rc != 0:
        return rc

    index = BUILD / "index.html"
    print(f"\nDocs built: {index}")
    if args.open:
        webbrowser.open(index.as_uri())
    return 0


if __name__ == "__main__":
    sys.exit(main())
