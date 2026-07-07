#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CSOURCE="$ROOT/csource"
PBX3CAGI="$CSOURCE/pbx3cagi"
LIB="$ROOT/tests/lib/agi_respond.py"

scenario="${1:?usage: run-scenario.sh <scenario-name>}"
scenario_dir="$ROOT/tests/scenarios/$scenario"

if [[ ! -d "$scenario_dir" ]]; then
  echo "unknown scenario: $scenario" >&2
  exit 1
fi

if [[ ! -x "$PBX3CAGI" ]]; then
  echo "building pbx3cagi..." >&2
  make -C "$CSOURCE"
fi

"$ROOT/tests/fixtures/setup-tenant-db.sh"

workdir="$(mktemp -d)"
transcript="$workdir/transcript.txt"
trap 'rm -rf "$workdir"' EXIT

export PBX3CAGI_SQLITE_DB="${PBX3CAGI_SQLITE_DB:-$ROOT/tests/fixtures/tenant/sqlite.rdonly.db}"

python3 "$LIB" "$scenario_dir" "$PBX3CAGI" "$transcript"
exit_code=$?

echo "--- transcript ($scenario) ---"
cat "$transcript"

fail=0
while IFS= read -r rule || [[ -n "$rule" ]]; do
  rule="${rule%%#*}"
  rule="$(echo "$rule" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
  [[ -z "$rule" ]] && continue

  case "$rule" in
    exit\ *)
      want="${rule#exit }"
      if [[ "$exit_code" -ne "$want" ]]; then
        echo "FAIL: expected exit $want got $exit_code" >&2
        fail=1
      fi
      ;;
    must-not\ *)
      pat="${rule#must-not }"
      if grep -Fq "$pat" "$transcript"; then
        echo "FAIL: must-not found: $pat" >&2
        fail=1
      fi
      ;;
    must\ *)
      pat="${rule#must }"
      if ! grep -Fq "$pat" "$transcript"; then
        echo "FAIL: must not found: $pat" >&2
        fail=1
      fi
      ;;
    *)
      echo "unknown expect rule: $rule" >&2
      fail=1
      ;;
  esac
done < "$scenario_dir/expect.txt"

if [[ "$fail" -ne 0 ]]; then
  exit 1
fi

echo "PASS: $scenario"
exit 0
