# build-doc

Scan all example READMEs and build the Sphinx documentation.

## Instructions

1. Install docs dependencies:
   ```bash
   pip install -r docs/requirements.txt
   ```

2. Build the docs from the repo root:
   ```bash
   sphinx-build -b html docs docs/_build
   ```
   `conf.py` automatically scans all `examples/{device,host,dual}/*/README.md`, copies them into `docs/examples/`, and regenerates `examples.rst` with the toctree.

3. Use a timeout of at least 60 seconds.

4. After the build completes:
   - Show the build output to the user.
   - Report total warnings and errors.
   - List which example READMEs were discovered and included.
   - If there are errors, suggest fixes.
