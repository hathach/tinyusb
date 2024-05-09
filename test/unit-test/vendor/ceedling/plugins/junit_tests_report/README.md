junit_tests_report
====================

## Overview

The junit_tests_report plugin creates an XML file of test results in JUnit
format, which is handy for Continuous Integration build servers or as input
into other reporting tools. The XML file is output to the appropriate
`<build_root>/artifacts/` directory (e.g. `artifacts/test/` for test tasks,
`artifacts/gcov/` for gcov, or `artifacts/bullseye/` for bullseye runs).

## Setup

Enable the plugin in your project.yml by adding `junit_tests_report`
to the list of enabled plugins.

``` YAML
:plugins:
  :enabled:
    - junit_tests_report
```

## Configuration

Optionally configure the output / artifact filename in your project.yml with
the `artifact_filename` configuration option. The default filename is
`report.xml`.

You can also configure the path that this artifact is stored. This can be done
by setting `path`. The default is that it will be placed in a subfolder under
the `build` directory.

``` YAML
:junit_tests_report:
  :artifact_filename: report_junit.xml
```
