# Espressif TinyUSB component

Upstream [TinyUSB](https://github.com/hathach/tinyusb) fork with integration into ESP-IDF build system.

## How to use

There are two options of using TinyUSB component with Espressif's SoCs:

### 1. Use component via [Espressif TinyUSB additions](https://github.com/espressif/esp-usb/tree/master/device/esp_tinyusb)

[Espressif TinyUSB additions](https://github.com/espressif/esp-usb/tree/master/device/esp_tinyusb) provides several preconfigured features to use benefits of TinyUSB stack faster.

To use [Espressif TinyUSB additions](https://github.com/espressif/esp-usb/tree/master/device/esp_tinyusb), add ``idf_component.yml`` to your main component with the following content::

```yaml
## IDF Component Manager Manifest File
dependencies:
  esp_tinyusb: "^1.0.0" # Automatically update minor releases
```

Or simply run:
```
idf.py add-dependency "esp_tinyusb^1.0.0"
```

Then, the Espressif TinyUSB component will be added automatically during resolving dependencies by the component manager.

### 2. Use component directly

Use this option for custom TinyUSB applications.
In this case you will have to provide configuration header file ``tusb_config.h``. More information about TinyUSB configuration can be found [in official TinyUSB documentation](https://docs.tinyusb.org/en/latest/reference/getting_started.html).

You will also have to tell TinyUSB where to find the configuration file. This can be achieved by adding following CMake snippet to you main component's ``CMakeLists.txt``:

```cmake
idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
target_include_directories(${tusb_lib} PRIVATE path_to_your_tusb_config)
```

Again, you can add this component to your project by adding ``idf_component.yml`` file:

```yaml
## IDF Component Manager Manifest File
dependencies:
  tinyusb: "~0.15.1" # Automatically update bugfix releases. TinyUSB does not guarantee backward compatibility
```

Or simply run:
```
idf.py add-dependency "tinyusb~0.15.1"
```

README from the upstream TinyUSB can be found in [hathach/tinyusb/README](https://github.com/hathach/tinyusb/blob/master/README.rst).
