#!/usr/bin/env bash

set -euo pipefail
SRC_DIR="build-debug"
DST_DIR="old_log"

timestamp=$(date +"%Y-%m-%d")

shopt -s nullglob

for file in "$SRC_DIR"/*.log; do
    filename=$(basename "$file")
    name="${filename%.log}"

    new_name="${name}_${timestamp}.log"

    mv "$file" "$DST_DIR/$new_name"
done
