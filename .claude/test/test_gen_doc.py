"""Tests for the reference-doc regeneration behind the build-doc skill:
get_deps.py --gen-doc writes the dependency table in the shape the committed
docs/reference/dependencies.rst has, without fetching anything; gen_doc.py
builds boards.rst from the metadata blocks and hil_boards.md from the rosters,
naming what it left out instead of dropping it silently."""
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GET_DEPS = ROOT / 'tools' / 'get_deps.py'
GEN_DOC = ROOT / '.claude' / 'skills' / 'build-doc' / 'scripts' / 'gen_doc.py'


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


get_deps = load('get_deps', GET_DEPS)
gen_doc = load('gen_doc', GEN_DOC)

METADATA_FAMILY = '/* metadata:\n   manufacturer: Acme\n*/\n#include "board.h"\n'
METADATA_BOARD = '/* metadata:\n   name: Acme One \n   url: https://acme.example/one\n*/\n#define BOARD_H_\n'


def fixture(root):
    """Two families: acme with one documented and one undocumented board, and nometa without a block."""
    bsp = root / 'hw' / 'bsp'
    (bsp / 'acme' / 'boards' / 'acme_one').mkdir(parents=True)
    (bsp / 'acme' / 'boards' / 'acme_bare').mkdir()
    (bsp / 'acme' / 'family.c').write_text(METADATA_FAMILY)
    (bsp / 'acme' / 'boards' / 'acme_one' / 'board.h').write_text(METADATA_BOARD)
    (bsp / 'acme' / 'boards' / 'acme_bare' / 'board.h').write_text('#define BOARD_H_\n')
    (bsp / 'nometa' / 'boards' / 'nometa_x').mkdir(parents=True)
    (bsp / 'nometa' / 'family.c').write_text('int x;\n')
    (bsp / 'nometa' / 'boards' / 'nometa_x' / 'board.h').write_text(METADATA_BOARD)
    (root / 'docs' / 'reference').mkdir(parents=True)
    (root / 'test' / 'hil').mkdir(parents=True)
    (root / 'test' / 'hil' / 'tinyusb.json').write_text(json.dumps({'boards': [
        {'name': 'acme_one', 'tests': {'device': True, 'host': True, 'dual': False},
         'flasher': {'name': 'jlink'}, 'variant': [{'name': 'acme_one_hs'}],
         'comment': 'needs  a | jumper'},
        {'name': 'acme_two', 'tests': {'only': ['host/cdc_msc', 'device/hid']}},
    ]}))
    (root / 'test' / 'hil' / 'hfp.json').write_text(json.dumps({'boards': []}))


class MetadataTest(unittest.TestCase):
    def test_block_values_are_stripped_and_missing_file_or_block_is_empty(self):
        with tempfile.TemporaryDirectory() as d:
            f = Path(d) / 'board.h'
            f.write_text(METADATA_BOARD)
            self.assertEqual(gen_doc.metadata(f), {'name': 'Acme One', 'url': 'https://acme.example/one'})
            f.write_text('/* metadata:\n   name:\n   url: https://acme.example\n*/\n')
            self.assertEqual(gen_doc.metadata(f), {'url': 'https://acme.example'})
            f.write_text('no block here\n')
            self.assertEqual(gen_doc.metadata(f), {})
            self.assertEqual(gen_doc.metadata(Path(d) / 'absent.h'), {})


class MdTableTest(unittest.TestCase):
    def test_github_table_pads_headers_by_two_and_strips_cells(self):
        self.assertEqual(gen_doc.md_table(['H', 'Name'], [['a ', 'long value'], ['bb', '']]).split('\n'), [
            '| H   | Name       |',
            '|-----|------------|',
            '| a   | long value |',
            '| bb  |            |',
        ])


class GenDocScriptTest(unittest.TestCase):
    def test_writes_the_three_files_and_lists_what_it_left_out(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            fixture(root)
            r = subprocess.run([sys.executable, str(GEN_DOC), '--root', str(root)], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            boards = (root / 'docs/reference/boards.rst').read_text()
            hil = (root / 'docs/reference/hil_boards.md').read_text()
            deps = (root / 'docs/reference/dependencies.rst').read_text()
        self.assertIn('Acme\n----\n\n', boards)
        self.assertIn('acme_one  Acme One  acme      https://acme.example/one', boards)
        self.assertNotIn('acme_bare', boards)
        self.assertNotIn('nometa', boards)
        self.assertTrue(boards.endswith('=\n'))
        self.assertEqual(r.stderr.split('\n'), [
            'no metadata block, left out of boards.rst:',
            '  hw/bsp/acme/boards/acme_bare',
            '  hw/bsp/nometa',
            '',
        ])
        self.assertTrue(hil.startswith(gen_doc.HEADER + '\n\n### ci rig\n\n2 boards, from `test/hil/tinyusb.json`.'))
        self.assertIn('| acme_one | device, host | jlink     | acme_one_hs | needs a \\| jumper |', hil)
        self.assertIn('| acme_two | device, host |           |             |                   |', hil)
        self.assertNotIn('hfp rig', hil)
        self.assertIn('lib/fatfs', deps)


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
