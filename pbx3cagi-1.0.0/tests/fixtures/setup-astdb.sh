#!/usr/bin/env bash
# Seed AstDB keys for scenarios (optional; scenarios use astdb.json by default).
set -euo pipefail

cat <<'EOF'
AstDB fixture for Phase 0 tests
================================

Production AstDB lives at /var/lib/asterisk/astdb.sqlite3 (SQLite on modern Asterisk).

Phase 0 does NOT open that file from pbx3cagi. The harness mock answers AGI
DATABASE GET from each scenario's astdb.json (family/key → value), e.g.:

  "cfim/59507r": "1102"

To capture live keys from golden after setting runtime CFIM in the SPA:

  asterisk -rx 'database show cfim'

Or copy astdb.sqlite3 for reference:

  cp /var/lib/asterisk/astdb.sqlite3 fixtures/astdb/astdb.sqlite3

The responder uses astdb.json only in Phase 0.
EOF
