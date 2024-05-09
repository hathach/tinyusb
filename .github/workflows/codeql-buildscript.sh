#!/usr/bin/env bash

FAMILY=stm32l4
pip install click
python3 tools/get_deps.py $FAMILY
python3 tools/build.py -s make $FAMILY
