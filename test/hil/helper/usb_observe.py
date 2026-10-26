#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bounded USB observation for the HIL harness (sysfs reads that give up on a wedged
device, the enumeration walk, the stray usbfs holder lookup): agentrc's shared module
(helper/agentrc.py). This name is registered as that module itself, the one agentrc's scripts
load too: one strand memo per process, and a monkeypatched root reaches every function."""
import sys

from . import agentrc

sys.modules[__name__] = agentrc.load_file('skills/hil/scripts/usb_observe.py')
