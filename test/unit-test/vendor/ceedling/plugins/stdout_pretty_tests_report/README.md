ceedling-pretty-tests-report
============================

## Overview

The stdout_pretty_tests_report is the default output of ceedling. Instead of
showing most of the raw output of CMock, Ceedling, etc., it shows a simplified
view. It also creates a nice summary at the end of execution which groups the
results into ignored and failed tests.

## Setup

Enable the plugin in your project.yml by adding `stdout_pretty_tests_report`
to the list of enabled plugins.

``` YAML
:plugins:
  :enabled:
    - stdout_pretty_tests_report
```
