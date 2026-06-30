#!/usr/bin/env python3
# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

import re
import shutil
from pathlib import Path

# -- Path setup --------------------------------------------------------------


# -- Project information -----------------------------------------------------

project = 'TinyUSB'
copyright = '2025, Ha Thach'
author = 'Ha Thach'


# -- General configuration ---------------------------------------------------

extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.intersphinx',
    'sphinx.ext.todo',
    'sphinx_autodoc_typehints',
    'myst_parser',          # Markdown (.md) support alongside reStructuredText
]

templates_path = ['_templates']

exclude_patterns = ['_build']


# -- Options for HTML output -------------------------------------------------

html_theme = 'furo'
html_title = 'TinyUSB'
html_logo = 'assets/logo.svg'
html_favicon = 'assets/logo.svg'
html_theme_options = {
    'sidebar_hide_name': True,
}
html_static_path = ['_static']
html_css_files = ['custom.css']

todo_include_todos = True

# pre-process path in README.rst
def preprocess_readme():
    """Modify figure paths in README.rst for Sphinx builds."""
    src = Path(__file__).parent.parent / "README.rst"
    tgt = Path(__file__).parent.parent / "README_processed.rst"
    if src.exists():
        content = src.read_text(encoding='utf-8')
        # if the matching is inside a table, keep the table cell width by adding the same number of spaces in the end of the line
        # match pattern: | ... `... <docs/...>`_ ... |
        # change into:   | ... `... <...>`_ ...      |
        def _rewrite_table_line(line):
            if not (line.startswith('|') and line.rstrip().endswith('|')):
                return line

            rewritten = re.sub(r"<docs/([^>]+)>", r"<\1>", line)
            delta = len(line) - len(rewritten) - 1  # -1 for rst->html

            if delta > 0:
                last_pipe = rewritten.rfind('|')
                if last_pipe >= 0:
                    rewritten = rewritten[:last_pipe] + (' ' * delta) + rewritten[last_pipe:]

            return rewritten

        content = ''.join(_rewrite_table_line(line) for line in content.splitlines(keepends=True))

        content = re.sub(r"docs/", r"", content)
        content = re.sub(r"\.rst\b", r".html", content)
        if not content.endswith("\n"):
            content += "\n"
        tgt.write_text(content, encoding='utf-8')

preprocess_readme()


# scan example READMEs into docs/examples/ and generate a per-group index page
EXAMPLE_GROUPS = ('device', 'host', 'dual')

_HEADING_RE = re.compile(r'^(#{1,6})(\s.*)$')
_FENCE_RE = re.compile(r'^\s*(```|~~~)')

def _normalize_headings(text):
    """Make each page a single Sphinx section: promote so the first heading is
    H1 and demote any later same-or-higher heading to at least H2. Without this,
    a README that uses flat #### headings (no H1) becomes several top-level
    sections and each leaks into the sidebar as a separate entry."""
    lines = text.splitlines(keepends=True)
    headings, in_fence = [], False
    for i, line in enumerate(lines):
        if _FENCE_RE.match(line):
            in_fence = not in_fence
        elif not in_fence and _HEADING_RE.match(line):
            headings.append(i)
    if not headings:
        return text
    delta = 1 - len(_HEADING_RE.match(lines[headings[0]]).group(1))
    for n, i in enumerate(headings):
        m = _HEADING_RE.match(lines[i])
        level = max(1, min(6, len(m.group(1)) + delta))
        if n > 0:
            level = max(level, 2)
        lines[i] = '#' * level + m.group(2) + ('\n' if lines[i].endswith('\n') else '')
    return ''.join(lines)

def _with_location(text, rel):
    """Insert a source-location note right after the first H1 so each rendered
    example page shows which example directory it came from."""
    note = f"> **Example source:** `{rel}`\n"
    lines = text.splitlines(keepends=True)
    for i, line in enumerate(lines):
        if line.lstrip().startswith("# "):
            return "".join(lines[:i + 1]) + "\n" + note + "\n" + "".join(lines[i + 1:])
    return note + "\n" + text  # no H1: prepend

def generate_examples_docs():
    """Copy every examples/{device,host,dual}/*/README.md into
    docs/examples/<group>/<name>.md (noting its source location) and write a
    docs/examples/<group>/index.rst landing page per group. index.rst points at
    those group pages, giving a 3-level sidebar: Examples > Device/Host/Dual >
    example. Output is rebuilt each run (git-ignored)."""
    docs_dir = Path(__file__).parent
    examples_root = docs_dir.parent / "examples"
    out_dir = docs_dir / "examples"

    # start clean so deleted/renamed examples don't leave stale pages
    if out_dir.exists():
        shutil.rmtree(out_dir)
    (docs_dir / "examples.rst").unlink(missing_ok=True)  # remove legacy single-file output

    for group in EXAMPLE_GROUPS:
        group_out = out_dir / group
        group_out.mkdir(parents=True, exist_ok=True)

        names = []
        for readme in sorted((examples_root / group).glob("*/README.md")):
            name = readme.parent.name
            rel = f"examples/{group}/{name}"
            content = _normalize_headings(readme.read_text(encoding='utf-8'))
            (group_out / f"{name}.md").write_text(_with_location(content, rel), encoding='utf-8')
            names.append(name)

        # group landing page (Device / Host / Dual) with a toctree of its examples
        heading = group.capitalize()
        page = [f"{'*' * len(heading)}\n{heading}\n{'*' * len(heading)}\n"]
        if names:
            page.append(".. toctree::\n   :maxdepth: 1\n")
            page.extend(f"   {name}" for name in names)
        else:
            page.append("No documented examples yet.")
        (group_out / "index.rst").write_text("\n".join(page) + "\n", encoding='utf-8')

generate_examples_docs()
