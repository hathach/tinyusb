ceedling-colour-report
======================

## Overview

The colour_report replaces the normal ceedling "pretty" output with
a colorized variant, in order to make the results easier to read from
a standard command line. This is very useful on developer machines, but
can occasionally cause problems with parsing on CI servers.

## Setup

Enable the plugin in your project.yml by adding `colour_report`
to the list of enabled plugins.

``` YAML
:plugins:
  :enabled:
    - colour_report
```
