# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------


# -- Project information -----------------------------------------------------

project = 'TinyUSB'
copyright = '2021, Ha Thach'
author = 'Ha Thach'


# -- General configuration ---------------------------------------------------

extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.intersphinx',
    'sphinx.ext.todo',
    'sphinx_autodoc_typehints',
    'sphinxemoji.sphinxemoji',
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
