warnings-report
===============

## Overview

The warnings_report captures all warnings throughout the build process
and collects them into a single report at the end of execution. It places all
of this into a warnings file in the output artifact directory.

## Setup

Enable the plugin in your project.yml by adding `warnings_report`
to the list of enabled plugins.

``` YAML
:plugins:
  :enabled:
    - warnings_report
```
