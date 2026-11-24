#!/usr/bin/env bash
# Usage: cache_key.sh NAME - print the cache key of toolchain NAME: the sha256 of its toolchain.json entry, so
# another entry's bump keeps its cache
set -euo pipefail

name=$1
manifest=$(dirname "$0")/../toolchain.json
jq -erj --arg n "$name" '.[$n].sha256 // error("no sha256 for \($n)")' "$manifest"
