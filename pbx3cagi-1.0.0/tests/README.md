# pbx3cagi Phase 0 — offline AGI tests

Run **`pbx3cagi`** without Asterisk: fixture tenant SQLite, mock AstDB on the AGI protocol, transcript assertions.

## Prerequisites

- **Linux** or **macOS** with `python3`, `gcc`, `make`
- **Linux:** `libbsd-dev` for `strlcpy`
- Tenant fixture: golden DB copy (see below)

## Quick start

```bash
cd pbx3cagi-1.0.0/csource && make
cd ../tests
./fixtures/setup-tenant-db.sh    # copies from pbx3/workingdocs/golden-sqlite.db if present
./run-scenario.sh cfim-local
./run-all-scenarios.sh
```

Or from `csource`:

```bash
make test
```

## Environment

| Variable | Purpose |
|----------|---------|
| `PBX3CAGI_SQLITE_DB` | Path to tenant read-only SQLite (default: `tests/fixtures/tenant/sqlite.rdonly.db`) |
| `GOLDEN_SQLITE` | Source path for `setup-tenant-db.sh` |

## Tenant fixture (deliverable 0.4)

From a dev machine with `pbx3-master`:

```bash
./fixtures/setup-tenant-db.sh
```

Or on golden:

```bash
cp /opt/pbx3/db/sqlite.rdonly.db tests/fixtures/tenant/sqlite.rdonly.db
```

## AstDB fixture (deliverable 0.5)

Scenarios use **`astdb.json`** per scenario (`family/key` → value). See `fixtures/setup-astdb.sh` for notes on live `astdb.sqlite3`.

## Scenarios

| Scenario | AstDB | Asserts |
|----------|-------|---------|
| `cfim-local` | CFIM → `1102` | No comfort tones; `SET EXTENSION 1102` |
| `cfim-external` | CFIM → `447700900123` | `Playback silence/1` + hold clip |
| `cfim-none` | empty | No forward branch |

Add scenarios under `scenarios/<name>/` with `agi_env.txt`, `argv.txt`, `astdb.json`, `expect.txt`.

## Layout

```
tests/
├── run-scenario.sh
├── run-all-scenarios.sh
├── lib/agi_respond.py
├── fixtures/
│   ├── setup-tenant-db.sh
│   └── tenant/sqlite.rdonly.db   (gitignored; copied locally)
└── scenarios/
    └── cfim-local/ ...
```

Spec: **`workingdocs/TEST_HARNESS.md`**
