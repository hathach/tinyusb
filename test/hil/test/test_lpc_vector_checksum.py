"""tools/lpc_vector_checksum.py: the NXP LPC boot checksum (vector table word 7) patched into an
ELF in place, found through the section that holds the given address. The ELFs are built here:
a header, one PROGBITS section and its section header table, which is all the tool reads."""
import contextlib
import io
import pathlib
import struct
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
import lpc_vector_checksum as lvc  # noqa: E402

SHT_NOBITS = 8


def make_elf(sections):
    """sections: [(sh_type, sh_addr, payload bytes[, sh_size])] -> little-endian ELF32 bytes"""
    body = b''
    entries = [b'\0' * 40]  # SHN_UNDEF
    offset = 52
    for sh_type, addr, payload, *size in sections:
        sh_size = size[0] if size else len(payload)
        entries.append(struct.pack('<10I', 0, sh_type, 0, addr, offset, sh_size, 0, 0, 4, 0))
        body += payload
        offset += len(payload)
    ident = b'\x7fELF' + bytes([1, 1, 1]) + b'\0' * 9
    header = ident + struct.pack('<HHIIIIIHHHHHH', 2, 40, 1, 0, 0, offset, 0, 52, 0, 0, 40, len(entries), 0)
    return header + body + b''.join(entries)


def vectors(word7=0):
    return struct.pack('<8I', 0x10008000, 0x1B000201, 0x1B000101, 0x1B000103,
                       0x1B000105, 0x1B000107, 0x1B000109, word7)


class Tool(unittest.TestCase):
    def run_tool(self, elf, *args):
        with tempfile.TemporaryDirectory() as d:
            path = pathlib.Path(d) / 'fw.elf'
            path.write_bytes(elf)
            err = io.StringIO()
            with contextlib.redirect_stderr(err):
                rc = lvc.main([str(path), *args])
            return rc, path.read_bytes(), err.getvalue()

    def test_checksum_zeroes_the_first_eight_words(self):
        words = list(struct.unpack('<8I', vectors()))
        words[7] = lvc.checksum(words)
        self.assertEqual(sum(words) & 0xFFFFFFFF, 0)

    def test_patches_word_7_of_the_bank_b_table_in_place(self):
        elf = make_elf([(lvc.SHT_PROGBITS, 0x1A000000, b'\xAA' * 64),
                        (lvc.SHT_PROGBITS, 0x1B000000, vectors() + b'\x55' * 32)])
        rc, out, _ = self.run_tool(elf, '--address', '0x1B000000')
        self.assertEqual(rc, 0)
        pos = lvc.find_vectors(out, 0x1B000000)
        words = struct.unpack_from('<8I', out, pos)
        self.assertEqual(sum(words) & 0xFFFFFFFF, 0)
        # only that word changed: the bank A section and the rest of the bank B one are untouched
        diff = [i for i, (a, b) in enumerate(zip(elf, out)) if a != b]
        self.assertTrue(diff and all(pos + 28 <= i < pos + 32 for i in diff), diff)

    def test_table_inside_a_section_not_at_its_start(self):
        elf = make_elf([(lvc.SHT_PROGBITS, 0x1AFFFFE0, b'\0' * 32 + vectors())])
        rc, out, _ = self.run_tool(elf, '--address', '0x1B000000')
        self.assertEqual(rc, 0)
        self.assertEqual(sum(struct.unpack_from('<8I', out, lvc.find_vectors(out, 0x1B000000))) & 0xFFFFFFFF, 0)

    def test_check_reports_an_invalid_table_without_writing(self):
        elf = make_elf([(lvc.SHT_PROGBITS, 0x1B000000, vectors())])
        rc, out, err = self.run_tool(elf, '--address', '0x1B000000', '--check')
        self.assertEqual(rc, 1)
        self.assertEqual(out, elf)
        self.assertIn('0x1B00001C', err)

    def test_check_passes_a_valid_table(self):
        words = list(struct.unpack('<8I', vectors()))
        elf = make_elf([(lvc.SHT_PROGBITS, 0x1B000000, vectors(lvc.checksum(words)))])
        self.assertEqual(self.run_tool(elf, '--address', '0x1B000000', '--check')[0], 0)

    def test_no_section_at_the_address_is_an_error(self):
        # bank A image only, and a NOBITS section covering the address does not count
        elf = make_elf([(lvc.SHT_PROGBITS, 0x1A000000, vectors()),
                        (SHT_NOBITS, 0x1B000000, b'', 64)])
        rc, out, err = self.run_tool(elf, '--address', '0x1B000000')
        self.assertEqual(rc, 2)
        self.assertEqual(out, elf)
        self.assertIn('0x1B000000', err)

    def test_table_cut_short_by_the_section_end_is_an_error(self):
        elf = make_elf([(lvc.SHT_PROGBITS, 0x1B000000, vectors()[:28])])
        self.assertEqual(self.run_tool(elf, '--address', '0x1B000000')[0], 2)

    def test_data_instead_of_a_vector_table_is_an_error(self):
        # e.g. a non-empty .text_Flash2 ahead of .text: the address holds code or data, not vectors
        elf = make_elf([(lvc.SHT_PROGBITS, 0x1B000000, b'\xAA' * 32)])
        rc, out, err = self.run_tool(elf, '--address', '0x1B000000')
        self.assertEqual((rc, out), (2, elf))
        self.assertIn('no vector table', err)

    def test_reset_vector_outside_the_bank_or_not_thumb_is_an_error(self):
        for reset in (0x1A000201, 0x1B000200):  # bank A handler; ARM-state (even) address
            words = list(struct.unpack('<8I', vectors()))
            words[1] = reset
            elf = make_elf([(lvc.SHT_PROGBITS, 0x1B000000, struct.pack('<8I', *words))])
            self.assertEqual(self.run_tool(elf, '--address', '0x1B000000')[0], 2, hex(reset))

    def test_not_an_elf32_le_file_is_an_error(self):
        self.assertEqual(self.run_tool(b'\x7fELF' + bytes([2, 1]) + b'\0' * 64, '--address', '0x1B000000')[0], 2)
        self.assertEqual(self.run_tool(b'not an elf' * 8, '--address', '0x1B000000')[0], 2)


if __name__ == '__main__':
    unittest.main()
