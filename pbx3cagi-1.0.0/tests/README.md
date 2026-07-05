# pbx3cagi Phase 0 — offline AGI tests

Run **`pbx3cagi`** without Asterisk: synthetic tenant SQLite (committed seed), mock AstDB on the AGI protocol, transcript assertions.

## Prerequisites

- **Linux** or **macOS** with `python3`, `gcc`, `make`, `sqlite3`
- **Linux:** `libbsd-dev` for `strlcpy`

## Quick start

```bash
cd pbx3cagi-1.0.0/csource && make
cd ../tests
./fixtures/setup-tenant-db.sh    # builds synthetic DB from minimal-tenant-seed.sql
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
| `PBX3CAGI_SQLITE_DB` | Path to tenant SQLite (default: `tests/fixtures/tenant/sqlite.rdonly.db`) |
| `GOLDEN_SQLITE` | If set, `setup-tenant-db.sh` copies this file instead of building synthetic (local only; **never commit** source) |
| `USE_GOLDEN_SQLITE=1` | Copy from `pbx3/workingdocs/golden-sqlite.db` or `/opt/pbx3/db/sqlite.rdonly.db` if present |

**Golden node QA:** point at the live read-only DB (does not use the synthetic seed):

```bash
export PBX3CAGI_SQLITE_DB=/opt/pbx3/db/sqlite.rdonly.db
./run-all-scenarios.sh
```

## Tenant fixture (synthetic, safe for Git)

Default: **`fixtures/minimal-tenant-seed.sql`** → `fixtures/tenant/sqlite.rdonly.db` (gitignored build output).

Synthetic IDs used by scenarios:

| Role | shortuid / pkey |
|------|-----------------|
| Tenant | `testtn01` / `tenant01` |
| Extension 1101 | `testex01` |
| Extension 1102 | `testex02` |

Add rows to **`minimal-tenant-seed.sql`** as new scenarios need more config. No real site or person names.

## AstDB

Scenarios use **`astdb.json`** per scenario (`family/key` → value). See `fixtures/setup-astdb.sh` for notes on live `astdb.sqlite3`.

## Scenarios

| Scenario | AstDB | Asserts |
|----------|-------|---------|
| `cfim-local` | CFIM → `1102` | No comfort tones; `SET EXTENSION 1102` |
| `cfim-external` | CFIM → `447700900123` | `Playback silence/1` + hold clip |
| `cfim-none` | empty | Normal LepDial; no CFIM forward |

## Layout

```
tests/
├── run-scenario.sh
├── run-all-scenarios.sh
├── lib/agi_respond.py
├── fixtures/
│   ├── minimal-tenant-seed.sql   (committed)
│   ├── setup-tenant-db.sh
│   └── tenant/sqlite.rdonly.db   (gitignored; built locally)
└── scenarios/
    └── cfim-local/ ...
```

Spec: **`workingdocs/TEST_HARNESS.md`**
