.. figure:: docs/assets/logo.svg
   :alt: TinyUSB

|Build Status| |Documentation Status| |License|

TinyUSB is an open-source cross-platform USB Host/Device stack for
embedded system, designed to be memory-safe with no dynamic allocation
and thread-safe with all interrupt events are deferred then handled in
the non-ISR task function.

Please take a look at the online `documentation <www.tinyusb.org>`__.

Local Docs
==========

- Info

  - `Uses`_
  - `Changelog`_
  - `Contributors`_

- `Reference`_

  - `Supported Devices`_
  - `Gettin Started`_
  - `Concurrency`_

- `Contributing`_

  - `Code of Conduct`_
  - `Structure`_
  - `Porting`_

License
=======

All TinyUSB sources in the ``src`` folder are licensed under MIT
license, `Full license is here <LICENSE>`__. However, each file can be
individually licensed especially those in ``lib`` and ``hw/mcu`` folder.
Please make sure you understand all the license term for files you use
in your project.


.. |Build Status| image:: https://github.com/hathach/tinyusb/workflows/Build/badge.svg
   :target: https://github.com/hathach/tinyusb/actions
.. |Documentation Status| image:: https://readthedocs.org/projects/tinyusb/badge/?version=latest
   :target: https://openinput.readthedocs.io/en/latest/?badge=latest
.. |License| image:: https://img.shields.io/badge/license-MIT-brightgreen.svg
   :target: https://opensource.org/licenses/MIT


.. _Uses: docs/info/uses.rst
.. _Changelog: docs/info/changelog.rst
.. _Contributors: CONTRIBUTORS.rst
.. _Reference: docs/reference/index.rst
.. _Supported Devices: docs/reference/supported.rst
.. _Gettin Started: docs/reference/getting_started.rst
.. _Concurrency: docs/reference/concurrency.rst
.. _Contributing: docs/contributing/index.rst
.. _Code of Conduct: CODE_OF_CONDUCT.rst
.. _Structure: docs/contributing/structure.rst
.. _Porting: docs/contributing/porting.rst
