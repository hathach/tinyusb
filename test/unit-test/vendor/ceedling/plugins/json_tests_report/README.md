json_tests_report
=================

## Overview

The json_tests_report plugin creates a JSON file of test results, which is
handy for Continuous Integration build servers or as input into other
reporting tools. The JSON file is output to the appropriate
`<build_root>/artifacts/` directory (e.g. `artifacts/test/` for test tasks,
`artifacts/gcov/` for gcov, or `artifacts/bullseye/` for bullseye runs).

## Setup

Enable the plugin in your project.yml by adding `json_tests_report` to the list
of enabled plugins.

``` YAML
:plugins:
  :enabled:
    - json_tests_report
```

## Configuration

Optionally configure the output / artifact filename in your project.yml with
the `artifact_filename` configuration option. The default filename is
`report.json`.

You can also configure the path that this artifact is stored. This can be done
by setting `path`. The default is that it will be placed in a subfolder under
the `build` directory.

``` YAML
:json_tests_report:
  :artifact_filename: report_spectuluarly.json
```
