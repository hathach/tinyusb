ceedling-teamcity-tests-report
==============================

## Overview

The teamcity_tests_report replaces the normal ceedling "pretty" output with 
a version that has results tagged to be consumed with the teamcity CI server.

## Setup

Enable the plugin in your project.yml by adding `teamcity_tests_report`
to the list of enabled plugins.

``` YAML
:plugins:
  :enabled:
    - teamcity_tests_report
```
