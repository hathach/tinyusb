ceedling-stdout-gtestlike-tests-report
======================

## Overview

The stdout_gtestlike_tests_report replaces the normal ceedling "pretty" output with
a variant that resembles the output of gtest. This is most helpful when trying to
integrate into an IDE or CI that is meant to work with google test.

## Setup

Enable the plugin in your project.yml by adding `stdout_gtestlike_tests_report`
to the list of enabled plugins.

``` YAML
:plugins:
  :enabled:
    - stdout_gtestlike_tests_report
```
