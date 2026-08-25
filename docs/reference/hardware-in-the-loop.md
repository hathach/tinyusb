# Hardware in the Loop (HIL)

Every pull request that touches code builds the examples and runs them on real silicon
before it can merge. This page documents the rigs that do it, in enough detail to
reproduce one.

Two rigs run the CI matrix:

| Rig   | Config                  | Runner labels                                           |
|-------|-------------------------|---------------------------------------------------------|
| `ci`  | `test/hil/tinyusb.json` | `self-hosted`, `X64`, `hathach`, `hardware-in-the-loop` |
| `hfp` | `test/hil/hfp.json`     | `self-hosted`, `Linux`, `X64`, `hifiphile`              |

`ci` is hathach's rig and is what the rest of this page describes. `hfp` is a similar VM
with a uPD720201 card, hosted by hifiphile.

## Bill of materials

| Part            | Used on `ci`                                                                     | Notes                                                                                   |
|-----------------|----------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------|
| Host PC         | Ryzen 9 3900X, MSI MAG B550M MORTAR WIFI, 32 GB                                  | Any x86 with a working IOMMU                                                            |
| USB controllers | 4 × Renesas uPD720201 (`1912:0014` rev 03) on one PCIe card                      | [SSU SU-U3244-12U][aio-card]: four controllers behind an on-board PCIe switch, 12 ports |
| Leaf hubs       | [MCS-92M 7-port USB 2.0 hub board][hub-board]                                    | XH2.54 headers instead of Type-A: sturdier under handling and far tidier to route       |
| Cables          | XH2.54 → [Type-C][cable-c] / [micro-B][cable-micro] pigtails                     | Hub-end pin order: `+`, `D−`, `D+`, `−`                                                 |
| Debug probes    | J-Link, ST-Link, RP2040 debug probe (CMSIS-DAP), WCH-Link, TI ICDI, ESP USB-JTAG | One per board — see Attached boards below                                               |
| USB fixtures    | Per host-capable board: one USB-serial adapter and one USB flash drive           | Only for boards that run host/dual tests — see below                                    |

[aio-card]: https://item.taobao.com/item.htm?id=990655153501
[hub-board]: https://item.taobao.com/item.htm?id=556123792554
[cable-c]: https://item.taobao.com/item.htm?id=826743445229
[cable-micro]: https://item.taobao.com/item.htm?id=591895354552

```{figure} ../assets/hil/pcie-card.jpg
:alt: Four-controller USB PCIe card
:width: 360px

One card, four uPD720201 controllers behind a PCIe switch.
```

```{figure} ../assets/hil/leaf-hub.jpg
:alt: MCS-92M leaf hub board
:width: 360px

One leaf hub: power in, upstream to a root port, seven XH2.54 ports out.
```

```{figure} ../assets/hil/cable-xh254.jpg
:alt: XH2.54 to USB-C pigtail
:width: 240px

Hub-end XH2.54, board-end USB — Type-C shown, micro-B is the same cable.
```

## Proxmox host

### 1. BIOS

Enable SVM (or VT-x/VT-d), IOMMU, and *Above 4G decoding*.

### 2. Kernel command line

In `/etc/default/grub`, then `update-grub`:

```
GRUB_CMDLINE_LINUX_DEFAULT="quiet iommu=pt pcie_acs_override=downstream,multifunction"
```

`pcie_acs_override` is required because the card's four controllers sit behind its own
PCIe switch, and that switch does not advertise ACS. Without the override all four land
in one IOMMU group and none can be passed through individually. It relaxes DMA isolation
between them — fine on a dedicated test rig, not on a shared host. Note it is a
Proxmox-kernel patch, not mainline: a stock kernel ignores it silently.

### 3. Bind the controllers to vfio-pci at boot

`/etc/modules`:

```
vfio
vfio_iommu_type1
vfio_pci
```

`/etc/modprobe.d/vfio.conf`:

```
options vfio-pci ids=1912:0014
softdep xhci_pci pre: vfio-pci
softdep xhci_pci_renesas pre: vfio-pci
```

Bind at boot, ahead of the host's xhci driver — do not rely on Proxmox's late binding.
If the host ever owns these ports, the constant failed enumerations from the boards keep
udev busy past 120 s, `udevadm settle` times out inside `ifupdown2-pre`,
`networking.service` is cancelled, and the host comes up with no network.

Then `update-initramfs -u -k all`, reboot, and check:

```bash
lspci -nnk -d 1912:0014 | grep -i 'kernel driver'   # vfio-pci
```

### 4. Pass the controllers to the VM

One `hostpci` entry per controller, not per card — take the BDFs from
`lspci -nn -d 1912:0014`:

```bash
qm set <vmid> --machine q35 --cpu host \
    --hostpci0 0000:07:00,pcie=1 --hostpci1 0000:08:00,pcie=1 \
    --hostpci2 0000:09:00,pcie=1 --hostpci3 0000:0a:00,pcie=1
```

`qm config <vmid>` should then list all four.

## Guest

Debian 13, 16 vCPU, 18 GB RAM.

### Renesas firmware

The controllers' ROM firmware is not reliable under HIL churn: Address Device fails with
`unexpected setup address command completion code 0x11`, and the controller eventually
dies outright (`xHCI host controller not responding, assume dead`). Install Renesas
firmware 2.0.2.6, which the kernel loads into the controller at boot.

Do this on the kernel that *binds* the controllers — with passthrough that is the guest,
not the Proxmox host.

1. Download 2.0.2.6 from [station-drivers][fw-dl]. It arrives as `k2026fwup1.exe`, a
   Windows self-extracting installer of 1,895,424 bytes. Verify the firmware it contains,
   not the installer — the md5 in the next step is the one that matters.
2. Unpack it — despite the name, the firmware inside is called `UPDATE.mem`:

   ```bash
   7z x k2026fwup1.exe -oupd      # or: cabextract -d upd k2026fwup1.exe
   md5sum upd/UPDATE.mem          # 11b49c68a400564b704c6ef17a0e6c0a, 13012 bytes
   ```

3. Install it under the name the kernel looks for, and rebuild the initramfs
   (`xhci-pci-renesas` lives there):

   ```bash
   sudo install -m 644 upd/UPDATE.mem /lib/firmware/renesas_usb_fw.mem
   sudo update-initramfs -u -k all
   sudo reboot
   ```

4. Confirm the controller is running it. The first check is the one
   `test/hil/usbtest.py` gates its own battery on — anything lower and it refuses to
   run, failing that board's `usbtest` cell:

   ```bash
   sudo setpci -s <bdf> 0x6c.l   # whole dword, must be >= 00202609
   dmesg | grep 'hcc params'     # 0x014051cf = firmware loaded, 0x014050cf = ROM fallback
   ```

The kernel reloads the firmware on every power cycle, so the file must stay installed —
that is what the initramfs step is for. The uPD720202 (`1912:0015`) takes the same
firmware and the same check.

A one-off `soft lockup` warning in `renesas_fw_download_image` while the firmware is
written is expected — it busy-waits over PCI config space for ~30 s.

[fw-dl]: https://www.station-drivers.com/index.php?option=com_remository&Itemid=353&func=fileinfo&id=1348&lang=en

### Software

| Purpose                     | What `ci` uses                                                                                                                            |
|-----------------------------|-------------------------------------------------------------------------------------------------------------------------------------------|
| Build                       | `cmake`, `ninja-build`, and a toolchain per family: `gcc-arm-none-eabi`, a RISC-V GCC, ESP-IDF                                            |
| Flashing                    | Five tools, one per `Flasher` value — see below                                                                                           |
| Test harness                | `pip install -r test/hil/requirements.txt` — hidapi, pyserial, esptool                                                                    |
| Host-side test tools        | `dfu-util`, `mtools`, `libmtp9`, `libmtp-runtime`, `alsa-utils` (apt) — the DFU, MSC, MTP and audio tests shell out to these              |
| USB inspection and recovery | `pciutils` (the `usbtest` firmware gate), `uhubctl` (apt), `tshark` for usbmon capture, `testusb` from the kernel's `tools/usb/testusb.c` |

The `Flasher` column in Attached boards names one of five values; only the ones your own
boards use have to be installed. The mapping is not always guessable:

| `Flasher`  | Binary                                                                                                                                                |
|------------|-------------------------------------------------------------------------------------------------------------------------------------------------------|
| `jlink`    | `JLinkExe`, from the SEGGER J-Link software                                                                                                           |
| `stlink`   | `STM32_Programmer_CLI`, from STM32CubeProgrammer — **not** `st-flash`                                                                                 |
| `openocd`  | [`hathach/openocd`][openocd-fork] branch `tinyusb` — one build merging the Raspberry Pi (RP2350), WCH and Analog Devices (MAX32) forks, none upstream |
| `esptool`  | `esptool` (pip)                                                                                                                                       |
| `lm4flash` | `lm4flash` (apt)                                                                                                                                      |

[openocd-fork]: https://github.com/hathach/openocd/tree/tinyusb

### Permissions and tools

```bash
sudo cp tools/88-tinyusb.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
# the groups 88-tinyusb.rules assigns; skip any the distro does not have
# (`wireshark` only exists once wireshark-common is installed)
for g in adm dialout plugdev users wireshark; do
  getent group "$g" >/dev/null && sudo usermod -aG "$g" "$USER"
done
```

Add the vendor rules for the probes you use (J-Link, picotool). `uhubctl` needs one too
and no package ships it — without it every port toggle wants root:

```
# /etc/udev/rules.d/52-uhubctl.rules - root hubs, plus each hub vendor in the rig
SUBSYSTEM=="usb", ATTR{idVendor}=="1d6b", MODE="0664", GROUP="plugdev"
SUBSYSTEM=="usb", ATTR{idVendor}=="1a40", MODE="0664", GROUP="plugdev"
SUBSYSTEM=="usb", ATTR{idVendor}=="045b", MODE="0664", GROUP="plugdev"
```

Flasher CLIs and toolchains must be reachable from *non-interactive* shells — neither the
Actions runner nor `hil_ci.sh` sources a login profile. Keep them in `~/.local/bin` and
`~/bin` (symlinks are fine) and add both to the runner's `.path`.

`pciutils` and passwordless sudo are hard requirements, not conveniences:
`test/hil/usbtest.py` shells out as `sudo -n` for `setpci`, `modprobe`, `dmesg` and
`testusb`, and exits outright if it cannot read the host controller's firmware version.
`helper/hil_pool_check.py` gates recovery on the same `sudo -n` plus
`.claude/skills/usb-kernel-recover/scripts/usb_recover.sh` being present; without both it
cannot re-authorize a wedged probe's port and files the board `flash-failed` instead.

The `usbtest` battery additionally needs `testusb` built from the kernel tools and
`CONFIG_USB_TEST=m` available.

## USB topology

**One 7-port hub per uPD720201 root port. Never chain hubs.**

Each controller presents four root ports (on both its USB 2 and USB 3 root hubs); the
card brings 12 of those 16 out to connectors. Hang exactly one leaf hub on a root port.

Boards are grouped into storage boxes, each holding **two** leaf hubs: one carries only
debug probes, the other only the boards under test. Keeping them apart is what makes
recovery tractable — a DUT re-enumerates constantly and can wedge its hub, while the
probes stay on a bus that never moves, so the probe you need to reset a hung board is
still there when you reach for it.

```{figure} ../assets/hil/storage-box.jpg
:alt: A storage box of boards, probes and two leaf hubs
:width: 800px

One box: boards, their probes, and the two leaf hubs serving them.
```

Boards that run **host** or **dual** tests additionally need a USB peripheral plugged
into the board's *own* USB port — a USB-serial adapter and/or a flash drive for the host
stack to enumerate. Ten `ci` boards have these, recorded as `dev_attached` in the rig
config and matched by exact VID:PID and serial, so a substitute part means updating the
config. The two Espressif
boards also use a TS3USB30 mux to drive device and host tests through one connector.

Why the rule matters:

- **Bandwidth.** Every leaf hub gets its own 480 Mbit uplink to the controller. Chaining
  puts a second hub's whole subtree behind one of those uplinks, and the `usbtest`
  battery saturates whatever it is given.
- **Blast radius.** A board that wedges its hub costs seven ports, not the rig.
- **Scheduling.** `hil_test.py` budgets flashing and `usbtest` concurrency per host
  controller (`test/hil/helper/hil_lock.py`: `FLASH_PARALLEL`, `USBTEST_PARALLEL`), which
  only means anything when a controller's set of devices is fixed.

Bus numbers are *not* stable across reboots or recabling, so nothing in the harness
addresses a board by bus path. Boards are identified by the MCU's unique ID and probes by
their serial, both recorded in the rig config — which is why every HIL board must
implement `board_get_unique_id()`.

## Attached boards

Roles come from each board's `tests` entry; `Flasher` is the tool that programs it.
Both files are the source of truth — this table is generated from them.

```{include} hil_boards.md
```

## How CI runs the tests

1. `hil-build` and `hil-build-esp` build the examples on GitHub-hosted runners and upload
   the binaries as artifacts.
2. `hil-tinyusb` runs on the self-hosted rigs, downloads those artifacts and calls
   `test/hil/hil_test.py`, which flashes each board and runs its tests. Espressif boards
   run in `hil-tinyusb-esp`, gated on the slower ESP-IDF build, and `hil-hfp-iar` builds
   with IAR inside the job.
3. On pull requests, `tools/ci_select.py` narrows the run to the boards a diff can
   affect — and each board's build to the examples its tests need — falling open to the
   full matrix when it cannot tell. The same pass scopes the build matrix.
4. Each board is arbitrated by a kernel flock in `/tmp/tinyusb-hil-locks/`, so interactive
   work and CI can share the rig without colliding.
5. Each rig job uploads its report as an artifact; `pr_comment.yml` downloads them and
   posts the combined tables onto the pull request.

From a development PC, the same run can be driven remotely. `REMOTE` and `CONFIG`
default to `ci`, so point them at your own:

```bash
REMOTE=myrig.lan CONFIG=$PWD/test/hil/local.json bash test/hil/hil_ci.sh -b <board>
```

## Gotchas

- **The Renesas firmware is not optional.** On ROM firmware these controllers fail Address
  Device and eventually die under test churn.
- **Port power is logical only.** `uhubctl` "off" on these controllers drops D+/D− but leaves
  VBUS hot — boards stay powered and running. Real per-port power switching needs the
  controller's PPON pins wired to load switches, which the card omits.
- **Use `uhubctl -S` on root ports.** Without it, uhubctl writes sysfs `disable`, which
  takes the root hub's lock — and if anything in that subtree is in D state it blocks
  there, leaving the whole bus untouchable. `-S` forces the libusb path instead, which is
  why `usb_recover.sh root-cycle` uses it. Resetting the board through its debug probe is
  the surer cure, but a wedged *probe* has none, so the port-side drop is the only lever
  left there.
- **Park firmware must busy-spin, never `wfe`/`wfi`.** A parked core in a low-power state
  can make SWD unreachable and leave the board needing recovery.
- **Most "7-port" hubs are two 4-port hubs in series.** Commodity 7-port hubs commonly
  cascade two controllers internally — three ports on the first, four behind a second.
  `lsusb -t` tells you which you bought: a single-tier hub appears as one device with
  seven ports, a cascaded one shows a hub inside a hub. Every hub on `ci` sits directly
  under a root port and reports `maxchild=7`.
- **Size the hub supplies.** Boards take VBUS from the leaf hub, so a seven-board hub on
  an undersized supply browns out under load.

## A minimal rig

None of the above is a prerequisite. The VM, the uPD720201 cards and the leaf hubs are
what let one machine hold 27 boards and recover them unattended — the harness itself runs
fine against boards plugged straight into a development PC's own USB ports, on whatever
xHCI that PC already has. All it takes is the boards, their debug probes, and a
`test/hil/local.json` describing them in the same shape as `tinyusb.json`.

Host-side prerequisites, beyond a cross toolchain:

```bash
python3 tools/get_deps.py <family>            # MCU SDKs for your boards
pip install -r test/hil/requirements.txt      # hidapi, pyserial, esptool
sudo apt install cmake ninja-build uhubctl \
                 dfu-util mtools libmtp9 libmtp-runtime alsa-utils
```

`cmake` and `ninja-build` are needed by any run and `uhubctl` by recovery; the rest only
by the tests that shell out to them, so dropping one just fails the DFU, MSC, MTP or audio
cells on an otherwise healthy rig. `test/hil/requirements.txt` names those at the top,
along with `iperf` for the `device/net_lwip_*` tests, which are off in the default matrix.

Only two of this page's host-controller concerns carry over. `test/hil/usbtest.py` refuses
a DUT behind a MosChip MCS9990 (`9710:9990`) outright, and it applies the Renesas firmware
check only when the DUT really is behind a uPD720201/02 — on a stock Intel or AMD xHCI
there is nothing to install, and `pciutils` is only needed for that check.

```bash
cd examples && cmake --preset <board> && cmake --build --preset <board>
cd .. && python3 test/hil/hil_test.py -B examples test/hil/local.json
```
