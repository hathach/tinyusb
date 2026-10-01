#!/usr/bin/env python3
"""Guards for sysview_record.py's host-side state handling."""
import os, sys, tempfile, unittest
from pathlib import Path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..',
                                '.claude', 'skills', 'sysview', 'scripts'))
import sysview_record as R


class StubDetection(unittest.TestCase):
    """A SIGKILLed run leaves our stub in place with its backup already consumed. The next run
    must not treat that stub as the user's config -- doing so lost a real 19 KB SystemView
    config permanently on the rig."""

    def test_our_stub_recognised(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "ini"
            p.write_text("[Preferences]\nLoadProjectOnStart=false\nSaveProperties=false\n")
            self.assertTrue(R._is_our_stub(p))

    def test_real_config_not_mistaken_for_stub(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "ini"
            p.write_text("[Preferences]\nLoadProjectOnStart=false\n" + "WindowState=@ByteArray(" +
                         "x" * 500 + ")\n")
            self.assertFalse(R._is_our_stub(p))

    def test_missing_file_is_not_a_stub(self):
        self.assertFalse(R._is_our_stub(Path("/nonexistent/ini")))


if __name__ == '__main__':
    unittest.main()
