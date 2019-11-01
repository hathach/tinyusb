ceedling-gcov
=============

# Plugin Overview

Plugin for integrating GNU GCov code coverage tool into Ceedling projects.
Currently only designed for the gcov command (like LCOV for example). In the
future we could configure this to work with other code coverage tools.

This plugin currently uses `gcovr` to generate HTML and/or XML reports as a
utility. The normal gcov plugin _must_ be run first for this report to generate.

## Installation

Gcovr can be installed via pip like so:

```
pip install gcovr
```

## Configuration

The gcov plugin supports configuration options via your `project.yml` provided
by Ceedling.

Generation of HTML reports may be enabled or disabled with the following
config. Set to `true` to enable or set to `false` to disable.

```
:gcov:
  :html_report: true
```

Generation of XML reports may be enabled or disabled with the following
config. Set to `true` to enable or set to `false` to disable.

```
:gcov:
  :xml_report: true
```

There are two types of gcovr HTML reports that can be configured in your
`project.yml`. To create a basic HTML report, with only the overall file
information, use the following config.

```
:gcov:
  :html_report_type: basic
```

To create a detailed HTML report, with line by line breakdown of the
coverage, use the following config.

```
:gcov:
  :html_report_type: detailed
```

There are a number of options to control which files are considered part of
the coverage report. Most often, we only care about coverage on our source code, and not
on tests or automatically generated mocks, runners, etc. However, there are times 
where this isn't true... or there are times where we've moved ceedling's directory 
structure so that the project file isn't at the root of the project anymore. In these
cases, you may need to tweak the following:

```
:gcov:
  :report_root: "."
  :report_exclude: "^build|^vendor|^test|^support"
  :report_include: "^src"
```

One important note about html_report_root: gcovr will only take a single root folder, unlike 
Ceedling's ability to take as many as you like. So you will need to choose a folder which is 
a superset of ALL the folders you want, and then use the include or exclude options to set up
patterns of files to pay attention to or ignore. It's not ideal, but it works.

Finally, there are a number of settings which can be specified in order to adjust the
default behaviors of gcov:

```
:gcov:
  :html_medium_threshold: 75
  :html_high_threshold: 90
  :fail_under_line: 30
  :fail_under_branch: 30
```

These HTML and XML reports will be found in `build/artifacts/gcov`.

## Example Usage

```
ceedling gcov:all utils:gcov
```

## To-Do list

- Generate overall report (combined statistics from all files with coverage)
- Generate coverage output files
- Easier option override for better customisation 
