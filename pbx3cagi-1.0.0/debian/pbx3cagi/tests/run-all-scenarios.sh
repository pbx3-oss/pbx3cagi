#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RUN="$ROOT/tests/run-scenario.sh"

fail=0
for dir in "$ROOT/tests/scenarios"/*/; do
  name="$(basename "$dir")"
  if ! "$RUN" "$name"; then
    fail=1
  fi
  echo
done

exit "$fail"
