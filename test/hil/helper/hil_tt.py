#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reset the hub transaction translator (TT) a full-/low-speed DUT sits behind: agentrc's
shared hil_tt (helper/agentrc.py), whose root child runs from lib/agentrc. This name is
registered as that module itself: one unconfirmed-hub memo per process, and a monkeypatched
SYS_USB or HELPER_TIMEOUT reaches every function. Callers pass `run=`, forwarding to
hil_util.run_cmd, so the child gets this harness's bounds."""
import sys

from . import agentrc

sys.modules[__name__] = agentrc.load_file('skills/hil/scripts/hil_tt.py')
