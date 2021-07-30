.. figure:: docs/assets/logo.svg
   :alt: TinyUSB

   TinyUSB

|Build Status| |Documentation Status| |License|

TinyUSB is an open-source cross-platform USB Host/Device stack for
embedded system, designed to be memory-safe with no dynamic allocation
and thread-safe with all interrupt events are deferred then handled in
the non-ISR task function.

Please take a look at the online `documentation <www.tinyusb.org>`__.

Contributors
============

Special thanks to all the people who spent their precious time and
effort to help this project so far. Check out the
`CONTRIBUTORS <CONTRIBUTORS.rst>`__ file for the list of all
contributors and their awesome work for the stack.

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
