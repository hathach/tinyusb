#!/usr/bin/env bash

FAMILY=stm32l4
python3 tools/get_family_deps.py $FAMILY
python3 tools/build_family.py $FAMILY
