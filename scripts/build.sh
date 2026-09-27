#!/usr/bin/env bash
# Configure and build the converter on macOS or Linux.
# Usage: scripts/build.sh [Release|Debug]
set -euo pipefail
cd "$(dirname "$0")/.."
CONFIG="${1:-Release}"
cmake -S . -B build -DCMAKE_BUILD_TYPE="$CONFIG"
cmake --build build --config "$CONFIG" --parallel
if [[ -f build/scratch2cpp ]]; then
    echo "Built: build/scratch2cpp"
elif [[ -f build/Release/scratch2cpp ]]; then
    echo "Built: build/Release/scratch2cpp"
else
    echo "Built. Look for scratch2cpp under build/"
fi
