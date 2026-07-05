# pbx3cagi — offline test recipe

**When you need it:** run Phase 0 AGI scenarios on **golden** or any Linux box (no live calls, no Asterisk required).

Full spec: **`TEST_HARNESS.md`**. Scenario details: **`pbx3cagi-1.0.0/tests/README.md`**.

---

## One-time setup (Linux)

From the repo root on the node:

```bash
cd pbx3cagi/pbx3cagi-1.0.0/csource
```

Install build deps (Debian/Ubuntu):

```bash
sudo apt-get update
sudo apt-get install -y build-essential libbsd-dev python3 sqlite3
```

Build the test binary:

```bash
make
```

---

## Default run (synthetic fixture — safe, no customer DB)

Uses committed **`tests/fixtures/minimal-tenant-seed.sql`**. Best for CI and everyday checks after code changes.

```bash
cd pbx3cagi/pbx3cagi-1.0.0/csource
make test
```

Or step by step:

```bash
cd pbx3cagi/pbx3cagi-1.0.0/tests
./fixtures/setup-tenant-db.sh
./run-all-scenarios.sh
```

**PASS** looks like:

```
PASS: cfim-external
PASS: cfim-local
PASS: cfim-none
```

Run one scenario:

```bash
cd pbx3cagi/pbx3cagi-1.0.0/tests
./run-scenario.sh cfim-local
```

---

## Golden run (live tenant SQLite)

Uses the node’s read-only pbx3 DB. AstDB is still mocked from each scenario’s **`astdb.json`** (synthetic keys `testex01`, etc.) — this mainly validates against real cluster/extension rows if you align IDs later.

```bash
cd pbx3cagi/pbx3cagi-1.0.0/tests
export PBX3CAGI_SQLITE_DB=/opt/pbx3/db/sqlite.rdonly.db
./run-all-scenarios.sh
```

Optional: copy a live DB into the fixture path instead of env override:

```bash
USE_GOLDEN_SQLITE=1 ./fixtures/setup-tenant-db.sh
# or: GOLDEN_SQLITE=/path/to/sqlite.rdonly.db ./fixtures/setup-tenant-db.sh
./run-all-scenarios.sh
```

**Never commit** copied golden/customer SQLite files.

---

## After pulling new code

```bash
cd pbx3cagi/pbx3cagi-1.0.0/csource
git pull
make clean && make test
```

---

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| `cannot find -lbsd` on Linux | `sudo apt-get install libbsd-dev` |
| `sqlite3: command not found` | `sudo apt-get install sqlite3` |
| Hang at start | AGI env must end with a **blank line** after variables (harness handles this) |
| `missing seed file` | Run from repo checkout; need `tests/fixtures/minimal-tenant-seed.sql` |
| Scenario FAIL | Read `--- transcript (name) ---` output; compare to `scenarios/<name>/expect.txt` |

---

## Scenarios (current)

| Name | What it checks |
|------|----------------|
| `cfim-local` | CFIM to extension `1102` — no hold clip |
| `cfim-external` | CFIM to PSTN-ish number — comfort tones play |
| `cfim-none` | No CFIM — normal `Dial PJSIP/testex01` |

Synthetic IDs: tenant **`testtn01`**, extension **`testex01`** / **`testex02`**. Extend **`minimal-tenant-seed.sql`** when adding scenarios.

---

*Last updated: 2026-07-04*
