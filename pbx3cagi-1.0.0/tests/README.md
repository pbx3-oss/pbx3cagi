# pbx3cagi Phase 0 — offline AGI tests

**Runbook (golden/Linux):** **`../../workingdocs/TEST_RECIPE.md`**

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

| Scenario | Command | Asserts (transcript) |
|----------|---------|----------------------|
| `cfim-none` | LepDial | PreDial sets `PBX3_DIAL` to singleton `PJSIP/…`; no `EXEC Dial` |
| `cfim-local` | LepDial | CFIM → local ext; no comfort tones |
| `cfim-external` | LepDial | CFIM → PSTN; hold clip |
| `lepdial-fleet` | LepDial + `PBX3_FLEET_MODE=1` | `PBX3_DIAL` includes `sip:suid@tenant.fqdn` |
| `lepdial-fleet-sitedial` | LepDial + fleet + `PBX3_SITE_DIAL=YES` | local `PJSIP/suid` only (no FQDN hairpin) |
| `dial-queue-predial` | Dial … queue | Phase E: `PBX3_DIAL` = `PJSIP/suid,,`; no `EXEC Dial` |
| `postdial-noanswer-vm` | PostDial + `DIALSTATUS=NOANSWER` | `EXEC Voicemail` |
| `postdial-answer-noop` | PostDial + `DIALSTATUS=ANSWER` | no Voicemail / Dial / Playback |
| `sched-open-legacy` | Ingress | No profile → openroute |
| `sched-closed-oclo` | Ingress + `seed_patch.sql` | closed → closeroute |
| `sched-open-profile` | Ingress | profile open → dest |
| `sched-mode-lunch-profile` | Ingress + patch | `sched_mode=lunch` → profile line |
| `force-master-closed-over-holiday` | Ingress | Q5 master `CLOSED` beats holiday dest |
| `force-master-lunch-over-holiday` | Ingress | Q5 master mode token `lunch` beats holiday |
| `force-tenant-closed-over-holiday` | Ingress | Tenant `OCSTAT=CLOSED` over holiday |
| `force-auto-resumes-sched` | Ingress | `AUTO` → follows `sched_mode` |
| `holiday-force-dest` | Ingress | Holiday dest when AUTO |
| `spy-default-denied` | `*68*1101` + patch `spy_pass=3333` | No Authenticate / ChanSpy (fail-closed) |
| `spy-ok` | `*68*1101` + patch `spy_pass=9911` | Authenticate then ChanSpy |

Optional per-scenario: **`variables.json`** (GET VARIABLE replies), **`env.json`** (process env), **`seed_patch.sql`**.

**Unit helpers:** from `csource/`, `make test-unit` (GetExt / Mangle / insecure feature-pass).

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
└── scenarios/<name>/
    ├── agi_env.txt
    ├── argv.txt
    ├── astdb.json
    ├── expect.txt
    ├── variables.json   (optional)
    └── env.json         (optional)
```

Spec: **`workingdocs/TEST_HARNESS.md`**
