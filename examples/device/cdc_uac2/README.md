#### Composite CDC + UAC2 on Pico

This example provides a composite CDC + UAC2 device on top of a Raspberry Pi
Pico board.


#### Use Cases

- The CDC + UAC2 composite device happens to be important, especially in the
  amateur radio community.

  Modern radios (`rigs`) like Icom IC-7300 + IC-705 expose a sound card and a
  serial device (`composite device`) to the computer over a single USB cable.
  This allows for Audio I/O and CAT control over a single USB cable which is
  very convenient.

  By including and maintaining this example in TinyUSB repository, we enable
  the amateur radio community to build (`homebrew`) radios with similar
  functionality as the (expensive) commercial rigs.

  This PR is important in bridging this specific gap between the commercial
  rigs and homebrew equipment.

- https://digirig.net/digirig-mobile-rev-1-9/ is a digital interface for
  interfacing radios (that lack an inbuilt digital interface) with computers.
  Digirig Mobile works brilliantly (is OSS!) and is a big improvement over
  traditional digital interfaces (like the SignaLink USB Interface). By using a
  Raspberry Pi Pico powered CDC + UAC2 composite device, we can simplify the
  Digirig Mobile schematic, drastically reduce the manufacturing cost, and
  (again) enable the homebrewers community to homebrew a modern digital interface
  with ease themselves.


#### Build Steps

```
cd examples/device/cdc_uac2

export PICO_SDK_PATH=$HOME/pico-sdk

cmake -DFAMILY=rp2040 pico .

cmake -DFAMILY=rp2040 -DCMAKE_BUILD_TYPE=Debug # use this for debugging

make BOARD=raspberry_pi_pico all
```


#### Development Notes

Please try to keep this code synchronized with the `uac2_headset` example
included in this repository.
