#!/usr/bin/env bash
# Build or refresh the local tenant SQLite fixture for offline tests.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
FIXTURE_DIR="$ROOT/tests/fixtures/tenant"
DB="$FIXTURE_DIR/sqlite.rdonly.db"
SEED="$ROOT/tests/fixtures/minimal-tenant-seed.sql"

mkdir -p "$FIXTURE_DIR"

if [[ -n "${GOLDEN_SQLITE:-}" && -f "${GOLDEN_SQLITE}" ]]; then
  cp "${GOLDEN_SQLITE}" "$DB"
  echo "copied tenant fixture from GOLDEN_SQLITE=${GOLDEN_SQLITE}"
  exit 0
fi

if [[ "${USE_GOLDEN_SQLITE:-}" == "1" ]]; then
  for src in \
    "$ROOT/../../pbx3/workingdocs/golden-sqlite.db" \
    "/opt/pbx3/db/sqlite.rdonly.db"; do
    if [[ -f "$src" ]]; then
      cp "$src" "$DB"
      echo "copied tenant fixture from ${src} (USE_GOLDEN_SQLITE=1)"
      exit 0
    fi
  done
  echo "USE_GOLDEN_SQLITE=1 but no source DB found" >&2
  exit 1
fi

if [[ ! -f "$SEED" ]]; then
  echo "missing seed file: $SEED" >&2
  exit 1
fi

rm -f "$DB"
sqlite3 "$DB" < "$SEED"

# Optional per-scenario patch (run-scenario exports SCENARIO_NAME)
if [[ -n "${SCENARIO_NAME:-}" ]]; then
  patch="$ROOT/tests/scenarios/${SCENARIO_NAME}/seed_patch.sql"
  if [[ -f "$patch" ]]; then
    sqlite3 "$DB" < "$patch"
    echo "applied seed_patch for ${SCENARIO_NAME}"
  fi
fi

echo "built synthetic tenant fixture from minimal-tenant-seed.sql"
