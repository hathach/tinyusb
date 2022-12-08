ceedling-raw-output-report
==========================

## Overview

The raw-output-report allows you to capture all the output from the called
tools in a single document, so you can trace back through it later. This is
useful for debugging... but can eat through memory quickly if left running.

## Setup

Enable the plugin in your project.yml by adding `raw_output_report`
to the list of enabled plugins.

``` YAML
:plugins:
  :enabled:
    - raw_output_report
```
