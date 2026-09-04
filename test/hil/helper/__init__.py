# Marks helper/ as a REGULAR package. Without this it is only a PEP 420 namespace portion,
# and a regular package named `helper` anywhere on sys.path wins over it even though
# test/hil is sys.path[0] -- one transitive pip install would break every HIL entry point
# at import. `helper` is a real distribution name on PyPI.
