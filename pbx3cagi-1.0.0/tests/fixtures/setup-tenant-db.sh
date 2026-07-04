#!/usr/bin/env bash
# Copy tenant SQLite fixture from golden export (not committed).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
FIXTURE_DIR="$ROOT/tests/fixtures/tenant"
DB="$FIXTURE_DIR/sqlite.rdonly.db"

mkdir -p "$FIXTURE_DIR"

if [[ -f "$DB" ]]; then
  echo "fixture already present: $DB"
  exit 0
fi

CANDIDATES=(
  "$ROOT/../../pbx3/workingdocs/golden-sqlite.db"
  "${GOLDEN_SQLITE:-}"
  "/opt/pbx3/db/sqlite.rdonly.db"
)

for src in "${CANDIDATES[@]}"; do
  [[ -n "$src" && -f "$src" ]] || continue
  cp "$src" "$DB"
  echo "copied tenant fixture from $src"
  exit 0
done

echo "No tenant DB found. Set GOLDEN_SQLITE or copy sqlite.rdonly.db to:" >&2
echo "  $DB" >&2
exit 1
