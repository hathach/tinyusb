#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bounded USB observation for the HIL harness (sysfs reads that give up on a wedged
device, the enumeration walk, the stray usbfs holder lookup): agentrc's shared module
(helper/agentrc.py). The loader registers it under this name, so callers get that module
itself: one strand memo per process, and a monkeypatched root reaches every function."""
from . import agentrc

agentrc.load('skills/hil/scripts/usb_observe.py', __name__)
