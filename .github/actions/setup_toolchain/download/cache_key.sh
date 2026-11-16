#!/usr/bin/env bash
# Usage: cache_key.sh NAME - print the cache key of toolchain NAME: the sha256 of each toolchain.json entry it
# installs, so another entry's bump keeps its cache; arm-iar also installs iar-lmsc-tools
set -euo pipefail

name=$1
manifest=$(dirname "$0")/../toolchain.json
jq -erj --arg n "$name" '
  def sha($e): .[$e].sha256 // error("no sha256 for \($e)");
  [sha($n), (if $n == "arm-iar" then sha("iar-lmsc-tools") else empty end)] | join("-")' "$manifest"
