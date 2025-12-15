#!/bin/bash

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SCRIPT_DIR="$ROOT_DIR/scripts"

cd "$ROOT_DIR"
echo "=== Running Reliability Test ==="

make clean
make || { echo "Reliability make failed"; exit 1; }
python3 "$SCRIPT_DIR/run.py" || { echo "Reliability test failed"; exit 1; }
python3 "$SCRIPT_DIR/parse_results.py" || { echo "Reliability result parsing failed"; exit 1; }