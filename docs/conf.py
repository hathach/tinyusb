#!/usr/bin/env python3
# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

import re
from pathlib import Path

# -- Path setup --------------------------------------------------------------


# -- Project information -----------------------------------------------------

project = 'TinyUSB'
copyright = '2024, Ha Thach'
author = 'Ha Thach'


# -- General configuration ---------------------------------------------------

extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.intersphinx',
    'sphinx.ext.todo',
    'sphinx_autodoc_typehints',
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

todo_include_todos = True

# pre-process path in README.rst
def preprocess_readme():
    """Modify figure paths in README.rst for Sphinx builds."""
    src = Path(__file__).parent.parent / "README.rst"
    tgt = Path(__file__).parent.parent / "README_processed.rst"
    if src.exists():
        content = src.read_text()
        content = re.sub(r"docs/", r"", content)
        content = re.sub(r".rst", r".html", content)
        tgt.write_text(content)

preprocess_readme()
