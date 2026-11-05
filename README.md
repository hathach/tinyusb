# Espressif TinyUSB component

[![Component Registry](https://components.espressif.com/components/espressif/tinyusb/badge.svg)](https://components.espressif.com/components/espressif/tinyusb)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)

## Overview

This repository is Espressif’s maintained fork of [TinyUSB](https://github.com/hathach/tinyusb), integrated with the ESP-IDF build system. It exists to provide ESP users with timely TinyUSB updates, as upstream releases occur too infrequently to match Espressif’s hardware cadence. All fixes and features developed in this repository are submitted upstream to maintain long-term alignment with the official TinyUSB project and minimize divergence between the two codebases.

Only Device part of TinyUSB stack is supported. For Host mode, please refer to [espressif/usb](https://components.espressif.com/components/espressif/usb) component.

#### Versioning and branching

**Fork vs upstream releases** — each Espressif tag is based on a matching [hathach/tinyusb](https://github.com/hathach/tinyusb) release. The table below links fork-specific changes (diffs) for each pair; add a row when a new release is cut.

| Upstream tag | Fork tag | IDF component | Diff |
| :--- | :--- | :--- | :--- |
| [`0.21.0`](https://github.com/hathach/tinyusb/releases/tag/0.21.0) | [`v0.21.0.1`](https://github.com/espressif/tinyusb/releases/tag/v0.21.0.1) | `0.21.0~1` | [compare](https://github.com/espressif/tinyusb/compare/hathach:tinyusb:0.21.0...v0.21.0.1) |
| [`0.21.0`](https://github.com/hathach/tinyusb/releases/tag/0.21.0) | [`v0.21.0.2`](https://github.com/espressif/tinyusb/releases/tag/v0.21.0.2) | `0.21.0~2` | [compare](https://github.com/espressif/tinyusb/compare/hathach:tinyusb:0.21.0...v0.21.0.2) |
| [`0.21.0`](https://github.com/hathach/tinyusb/releases/tag/0.21.0) | [`v0.21.0.3`](https://github.com/espressif/tinyusb/releases/tag/v0.21.0.3) | `0.21.0~3` | [compare](https://github.com/espressif/tinyusb/compare/hathach:tinyusb:0.21.0...v0.21.0.3) |
| [`0.21.0`](https://github.com/hathach/tinyusb/releases/tag/0.21.0) | [`v0.21.0.4`](https://github.com/espressif/tinyusb/releases/tag/v0.21.0.4) | `0.21.0~4` | [compare](https://github.com/espressif/tinyusb/compare/hathach:tinyusb:0.21.0...v0.21.0.4) |

TinyUSB has not yet reached its first stable release (v1.0.0), so breaking changes may still occur in any version. In practice, the API remains relatively stable, and when incompatible changes arise, we aim to preserve backward compatibility where feasible.

#### Examples

USB Device examples based on TinyUSB are present in [esp-idf](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/device).

## How to use

There are two options of using TinyUSB component with Espressif's SoCs:

### 1. Use component via [esp_tinyusb](https://components.espressif.com/components/espressif/esp_tinyusb)

[Espressif TinyUSB additions](https://github.com/espressif/esp-usb/tree/master/device/esp_tinyusb) (esp_tinyusb) provide several preconfigured features to use benefits of TinyUSB stack faster.

To use [Espressif TinyUSB additions](https://github.com/espressif/esp-usb/tree/master/device/esp_tinyusb), add ``idf_component.yml`` to your main component with the following content::

```yaml
## IDF Component Manager Manifest File
dependencies:
  esp_tinyusb: "^3.0.0" # Automatically update minor releases
```

Or simply run:
```sh
idf.py add-dependency "esp_tinyusb^3.0.0"
```

### 2. Using standalone TinyUSB

Use this option for custom TinyUSB applications.
In this case you will have to provide configuration header file `tusb_config.h`. More information about TinyUSB configuration can be found [in official TinyUSB documentation](https://docs.tinyusb.org/en/latest/getting_started.html).

You will also have to tell TinyUSB where to find the configuration file. This can be achieved by adding following CMake snippet to your main component's ``CMakeLists.txt``:

```cmake
idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
target_include_directories(${tusb_lib} PRIVATE path_to_your_tusb_config)
```

Again, you can add this component to your project by adding ``idf_component.yml`` file:

```yaml
## IDF Component Manager Manifest File
dependencies:
  tinyusb: "~0.21.0" # TinyUSB does not guarantee backward compatibility
```

Or simply run:
```sh
idf.py add-dependency "tinyusb~0.21.0"
```

README from the upstream TinyUSB can be found in [hathach/tinyusb/README](https://github.com/hathach/tinyusb/blob/master/README.rst).
