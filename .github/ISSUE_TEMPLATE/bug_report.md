---
name: Bug Report
about: Create a report to help us improve
title: 'Please provide all details at least for Setup/Describe/Reproduce'
labels: Bug 🐞
assignees: ''

---

**Set Up**

- **PC OS** e.g Ubuntu 20.04 / Windows 10/ macOS 10.15
- **Board** e.g Feather nRF52840 Express (if custom specify your MCUs)
- **TinyUSB version** relase version or git hash (preferrably running with master for lastest code) 
- **Firmware** e.g examples/device/cdc_msc

**Describe The Bug**

A clear and concise description of what the bug is.

**To Reproduce**

Steps to reproduce the behavior:
1. Go to '...'
2. Click on '....'
3. See error

**Screenshots**

If applicable, add screenshots, bus capture to help explain your problem. 

**Log**

If applicable, provide the stack's log (uart/rtt/swo) where the issue occurred, best with comments to explain the actual events. If the log is too long, attach it as txt file instead.

Note: To enable logging, add `LOG=2` to to the make command if building with stock examples or set `CFG_TUSB_DEBUG=2` in your tusb_config.h. More information can be found at [example's readme](/docs/getting_started.md)
