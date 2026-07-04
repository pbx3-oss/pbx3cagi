# pbx3cagi Phase 0 — AGI test harness

**Status:** Required before further refactor phases (see **`REFACTOR_PLAN.md`** § Phase 0).

**Problem:** `pbx3cagi` only runs during a call. Asterisk forks one process per AGI invocation; logic is spread across globals, pbx3 SQLite, AstDB (via AGI), and channel `EXEC` side effects. Manual phone testing does not scale.

**Goal:** Run **`pbx3cagi` offline** — one process per scenario — with fixture databases and a scripted AGI peer. Assert on **stdout transcript** (and exit code), not on real channels.

**Non-goals (Phase 0):** FastAGI server, persistent multiplexer, reentrant code, full IVR multi-turn library, SIP/audio, or replacing golden operator QA.

---

## Architecture

```
┌─────────────────┐     stdin (AGI env + mock 200 responses)
│  agi_respond    │◄────────────────────────────────────────┐
│  (parent)       │                                          │
└────────┬────────┘                                          │
         │ pipes                                             │
         ▼                                                   │
┌─────────────────┐     stdout (EXEC, DATABASE GET, …)      │
│  pbx3cagi       │──────────────────────────────────────────┘
│  (child)        │
└────────┬────────┘
         │ opens read-only
         ▼
   fixture tenant SQLite          fixture AstDB SQLite
   (golden copy)                  (queried by responder on DATABASE GET)
```

- **One child process per scenario** — same isolation as production; no reentrancy requirement.
- **Standard AGI pipe mode** — no FastAGI required (optional later).
- **Parent** feeds AGI environment block + answers `DATABASE GET` / `GET VARIABLE` / `EXEC` follow-ups with canned `200` lines (AstDB lookups from fixture file).

---

## Three fixture layers

| Layer | Source | Phase 0 approach |
|-------|--------|------------------|
| **1. Tenant DB** | `/opt/pbx3/db/sqlite.rdonly.db` | Copy from golden (`pbx3/workingdocs/golden-sqlite.db` or node export). Tests point at copy via env (see deliverables). |
| **2. AstDB** | Asterisk `astdb.sqlite3` | Copy + seed keys (`/cfim/<shortuid>`, agent login, etc.). Responder answers `DATABASE GET family key` from SQLite or a small key→value map. **`pbx3cagi` does not open AstDB directly today** — mock is on the AGI protocol, not a code path change (optional test shim later). |
| **3. Channel** | Asterisk channel | **Transcript only** — assert presence/absence of `EXEC Playback …`, `SET EXTENSION`, etc. Default mock: `200 result=0` for benign `EXEC` / `SET` / channel status. |

---

## Repository layout (target)

```
pbx3cagi-1.0.0/tests/
├── README.md                 # how to run on Linux / golden
├── run-scenario.sh           # entry: ./run-scenario.sh scenarios/cfim-local
├── lib/
│   └── agi_respond.py        # read child stdout, write stdin replies
├── fixtures/
│   ├── tenant/
│   │   └── sqlite.rdonly.db  # git-lfs or generate script; not committed if large
│   └── astdb/
│       └── astdb.sqlite3     # seeded subset
└── scenarios/
    └── cfim-local/
        ├── agi_env.txt       # AGI variable block + trailing blank line
        ├── argv.txt          # LepDial cfim <shortuid> <cluster> …
        ├── astdb.json        # optional flat map: "cfim/9wvvnb" → "1102"
        └── expect.txt        # patterns: must / must-not (grep -E)
```

---

## Scenario file formats

### `agi_env.txt`

Lines Asterisk sends before the blank line, e.g.:

```
agi_network: no
agi_channel: Local/test@default-00000001;1
agi_context: 9wvvnb
agi_extension: 1101
agi_callerid: 1100
agi_calleridname: Test Caller
agi_uniqueid: test.001
agi_dnid: 1101
agi_rdnis: unknown
agi_type: Local
agi_language: en

```

**Important:** include a **blank line** after the last variable line. `AGITool_Init` reads until a line that is exactly `\n`; without it, `pbx3cagi` blocks on stdin (harness deadlock).

### `argv.txt`

Whitespace-separated args **after** program name (same as dialplan AGI args):

```
LepDial cfim 9wvvnb 9wvvnb
```

### `expect.txt`

One rule per line:

```
must-not EXEC Playback pls-hold-while-try
must SET EXTENSION 1102
must SET CONTEXT
exit 0
```

---

## Phase 0 deliverables (acceptance criteria)

| # | Deliverable | Done when |
|---|-------------|-----------|
| **0.1** | **Test DB path override** | Env `PBX3CAGI_SQLITE_DB` (or documented symlink-only workaround) so tests do not require writing `/opt/pbx3/db/`. |
| **0.2** | **`run-scenario.sh`** | Runs one scenario: spawn `pbx3cagi` with fixture env + argv; capture stdout/stderr; exit code recorded. |
| **0.3** | **`agi_respond.py`** | Parses child AGI commands on stdout; replies on stdin; `DATABASE GET` served from `astdb.json` or fixture SQLite. |
| **0.4** | **Golden tenant fixture** | Documented copy/export procedure from **08jzwn** (or use existing golden DB copy). |
| **0.5** | **AstDB fixture + seed doc** | How to copy `astdb.sqlite3` and seed `/cfim/<shortuid>` for extension tests. |
| **0.6** | **Scenario: CFIM local** | Forward target `1102` → no comfort `Playback`; sets extension/context (regression for **1.0.0-2 CFCheck**). |
| **0.7** | **Scenario: CFIM external** | Forward target long PSTN-ish number → `Playback silence/1` (+ `pls-hold-while-try` when `playtransfer=YES`). |
| **0.8** | **CI / developer note** | `tests/README.md`: run on Linux arm64/amd64 after `make`; optional `make test` target. |

**Phase 0.1 (follow-on, not blocking refactor):** additional scenarios (Ingress cluster, CheckState, one IVR branch); multi-turn responder for `GET DATA`.

---

## First scenarios (priority)

1. **`cfim-local`** — AstDB `cfim/<shortuid>` → `1102`; assert no hold clip.
2. **`cfim-external`** — AstDB → `447700900123`; assert comfort tones when enabled.
3. **`cfim-none`** — empty AstDB; assert no forward branch.
4. **`ingress-cluster`** — argv `PARM_CLST` vs `agi_context` (shortuid resolution); SQLite-only assertions where possible.

---

## Relationship to refactor phases

- **Phase 0 is mandatory** before Phase 1.1+ structural refactors (`REFACTOR_PLAN.md`).
- Each refactor commit should run the scenario suite (or relevant subset).
- Phase 3 “AGI abstraction” becomes easier once the harness exists — swap real responder for mock `agi_session_t`.

---

## References

- **`REFACTOR_PLAN.md`** — Phase 0 requirement and order of work
- **`pbx3/workingdocs/TODO.md`** — open tracking item
- Production AGI init: `AGITool_Init` (`cagi.c`) — stdin env block
- AstDB in live system: Asterisk `astdb.sqlite3`; access in code via `DBGet` / `DBPut` / `DBDel` only
