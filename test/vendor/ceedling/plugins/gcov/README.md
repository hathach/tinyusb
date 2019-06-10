ceedling-gcov
=============

Plugin for integrating GNU GCov code coverage tool into Ceedling projects.
Currently only designed for the gcov command (like LCOV for example). In the
future we could configure this to work with other code coverage tools.


This plugin currently uses `gcovr` to generate HTML reports as a utility. The
normal gcov plugin _must_ be run first for this report to generate.

Gcovr can be installed via pip like so:

```
pip install gcovr
```

There are two types of gcovr HTML reports that can be configured in your
`project.yml`. To create a basic HTML report with only the overall file
information use the following config. 

```
:gcov:
  :html_report_type: basic
```
To create a detailed HTML report with line by line breakdown of the coverage use
the following config.

```
:gcov:
  :html_report_type: detailed
```

These reports will be found in `build/artifacts/gcov`.



# To-Do list

- Generate overall report (combined statistics from all files with coverage)
- Generate coverage output files
- Easier option override for better customisation 
