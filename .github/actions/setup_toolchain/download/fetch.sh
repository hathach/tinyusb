#!/usr/bin/env bash
# Usage: fetch.sh NAME OUT - download toolchain.json's NAME entry over https only and verify its sha256
set -euo pipefail

name=$1 out=$2
manifest=$(dirname "$0")/../toolchain.json
url=$(jq -ej --arg n "$name" '.[$n].url' "$manifest")
sha256=$(jq -ej --arg n "$name" '.[$n].sha256' "$manifest")

curl -fsSL --proto '=https' --proto-redir '=https' --retry 5 --connect-timeout 30 --speed-limit 1024 --speed-time 60 "$url" -o "$out"

# macOS runners have shasum but no sha256sum
if command -v sha256sum > /dev/null; then
  echo "$sha256  $out" | sha256sum -c -
else
  echo "$sha256  $out" | shasum -a 256 -c -
fi
