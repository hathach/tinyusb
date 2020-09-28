---
name: Bug report
about: Create a report to help us improve
title: ''
labels: Bug 🐞
assignees: ''

---

**Set up**
[Mandatory] Provide details of your setup help us to reproduce the issue as quick as possible  
 - **PC OS**   : Ubuntu 18.04 / Windows 10/ macOS 10.15 
 - **Board**   : Feather nRF52840 Express
 - **Firmware**: examples/device/cdc_msc

**Describe the bug**
A clear and concise description of what the bug is.

**To reproduce**
Steps to reproduce the behavior:
1. Go to '...'
2. Click on '....'
3. See error

**Screenshots**
If applicable, add screenshots, bus capture to help explain your problem. 

**Log**
Please provide the stack's log (uart/rtt/swo) where the issue occurred, best with comments to explain the actual events. To enable logging, add `LOG=2` to to the make command if building with stock examples or set `CFG_TUSB_DEBUG=2` in your tusb_config.h. More information can be found at [example's readme](/docs/getting_started.md)
