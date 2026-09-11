"""Tests for the reference-doc regeneration behind the build-doc skill:
get_deps.py --gen-doc writes the dependency table in the shape the committed
docs/reference/dependencies.rst has, without fetching anything."""
import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GET_DEPS = ROOT / 'tools' / 'get_deps.py'
spec = importlib.util.spec_from_file_location('get_deps', GET_DEPS)
get_deps = importlib.util.module_from_spec(spec)
spec.loader.exec_module(get_deps)


class RstTableTest(unittest.TestCase):
    def test_columns_are_two_apart_and_headers_two_wider_than_their_text(self):
        table = get_deps.rst_table(['H1', 'Name'], [['a', 'long value'], ['ccc', 'd']])
        self.assertEqual(table.split('\n'), [
            '====  ==========',
            'H1    Name',
            '====  ==========',
            'a     long value',
            'ccc   d',
            '====  ==========',
        ])

    def test_cells_are_stripped_and_no_line_ends_in_a_space(self):
        table = get_deps.rst_table(['A', 'B'], [['x', 'tm4c '], ['yy', '']])
        self.assertEqual(table.split('\n')[0], '===  ====')
        self.assertFalse(any(line.endswith(' ') for line in table.split('\n')))

    def test_no_rows_gives_a_header_between_three_rules(self):
        self.assertEqual(get_deps.rst_table(['A'], []), '===\nA\n===\n===')


class GenDocTest(unittest.TestCase):
    def test_gen_doc_writes_every_dependency_sorted_by_path(self):
        with tempfile.TemporaryDirectory() as d:
            out = Path(d) / 'deps.rst'
            r = subprocess.run([sys.executable, str(GET_DEPS), '--gen-doc', str(out)],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            text = out.read_text()
        self.assertTrue(text.startswith('************\nDependencies\n************\n'))
        self.assertTrue(text.endswith('=\n'))
        paths = [line.split()[0] for line in text.split('\n')
                 if line.startswith(('hw/', 'lib/', 'tools/'))]
        self.assertEqual(paths, sorted(get_deps.deps_all))
        self.assertIn('Local Path', text)
        self.assertNotIn('cloning', r.stdout)

    def test_gen_doc_defaults_to_the_docs_reference_file(self):
        r = subprocess.run([sys.executable, str(GET_DEPS), '--help'], capture_output=True, text=True)
        self.assertIn('docs/reference/dependencies.rst', r.stdout)
        self.assertEqual(get_deps.DEPS_RST, ROOT / 'docs/reference/dependencies.rst')


if __name__ == '__main__':
    unittest.main()
