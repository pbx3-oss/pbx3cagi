# pbx3cagi Refactor Plan

## Agreed route forward

1. **Ship the cleanup first.** The changes already made (compile fixes, braces, typos, strlcpy, semicolon, pkey→key, sign-compare casts, bsd_compat, header prototypes) are committed. Deploy that build, get it working in the target environment, and complete the required testing. Establish a **stable, known-good baseline**.
2. **Phase 0 (required): AGI test harness.** Before structural refactor (Phase 1.1+), implement offline scenario tests — fixture tenant SQLite, AstDB mock on the AGI protocol, transcript assertions. See **`TEST_HARNESS.md`**; run on golden with **`TEST_RECIPE.md`**. One `pbx3cagi` process per scenario (same as production); no FastAGI multiplexer, no reentrancy change.
3. **Refactor in phases; test after each phase.** Run the Phase 0 scenario suite after each refactor step. Do e.g. Phase 1.3 (dead code), test; then Phase 1.1 (structs), test; then next phase.
4. **Fallback.** Always have a recent known-good state (tag or branch) to revert to.

---

## Current State

- **pbx3cagi.c**: ~2,790 lines (+ `agi_sqlite.c` for tenant SQLite).
- **~35 global variables** (call state, cluster, channel, DB result buffer, AGI args, debug).
- **~45 functions**: `main()` dispatches via `agi_cmd_table[]` (Phase 2.1); command handlers in `pbx3cagi.c`; SQLite binds / `load_cluster_cfg` in `agi_sqlite.c`.
- **Tight coupling**: every command handler reads/writes globals (`agi`, `res`, `myCluster`, `rescols[]`, `callerid`, etc.) and calls shared DB/AGI helpers.

### Data access: two separate systems

- **Asterisk DB** (DBGet, DBPut, DBDel): Calls into *Asterisk’s* internal database via AGI (DATABASE GET/PUT/DEL). Used for runtime call state, agent login (e.g. DYNLOGIN, eAgent, dAgent), and similar. On the Asterisk side AstDB is persisted (SQLite in modern builds); **`pbx3cagi` only sees it through AGI**, not by opening `astdb.sqlite3` directly. Not the pbx3 tenant schema.
- **SQLite (pbx3 tenant DB):** Opens the *pbx3* read-only SQLite DB in-process (e.g. `/opt/pbx3/db/sqlite.rdonly.db`). **Runtime `SELECT`s** use **`sqlQueryBind1` / `sqlQueryBind2`** (internal `sqlQueryBindInternal`): SQL uses `?` placeholders; results land in global **`rescols[]`**. One **shared `sqlite3` connection** per AGI process (opened on first use, `atexit` closes on normal exit). The old string-interpolation helper path **`sqlQuery()` has been removed**; **`DBQuery` / `sqlSelectEq` are gone**. **Cluster row:** `load_cluster_cfg()` still uses **direct `sqlite3_*`** for the single `SELECT` that fills `g_cluster_cfg` (escaped literals for `pkey`/`shortuid`).

Refactor steps should treat these as distinct: “SQLite module” = pbx3 data only; “Asterisk DB” stays as AGI wrappers. Public SQLite read API today: **`sqlQueryBind1` / `sqlQueryBind2`** (+ `load_cluster_cfg` for tenant config).

### Main structural elements

| Section              | Approx lines | Role |
|----------------------|-------------|------|
| Globals + cfTab      | 1–80        | Call context, AGI args, constants |
| main()               | (see source) | Init, PARM_CLST → myCluster, cmd table dispatch |
| setMoh               | (see source) | MOH from cluster cfg |
| Helpers (GetExt, Mangle, Auth, RecGreet, etc.) | 562–1085 | Small utilities + Agent/ChanSpy |
| OutRoute             | 1086–1306   | Outbound routing |
| OutTrunk / OutVoip   | 1308–1478   | Trunk/VoIP dial |
| InCall               | 1480–1716   | Inbound call handling |
| Dial                 | 1718–1800   | Generic dial (queue, etc.) |
| SetRecord, Page      | 1802–1986   | Recording, paging |
| Call forwarding (CF*) | 1988–2190  | CFVMail, CFToggle, FollowMe, CFOff |
| SetRingDelay, StripPreselect, SetTimer | 2191–2262 | Timers, preselect |
| Inbound              | 2264–2425   | Inbound route handling |
| CheckState, CheckTime, routeClass | 2427–2666 | Time/state, route class |
| IVR / IVRAction      | 2668–2888   | IVR menus |
| **Asterisk DB** (DBGet, DBPut, DBDel) | 2890–2912 | Asterisk internal DB via AGI (DATABASE GET/PUT/DEL) – call state, agent login, etc. |
| **SQLite** (`agi_sqlite.c`: bind helpers + `load_cluster_cfg`) | (see `agi_sqlite.c`) | pbx3 tenant data; results in `rescols[]` |
| OutQmt, QLogWrite, outboundClip, consoleMsg | 3133–3325 | Queue log, clip, logging |

---

## Pain Points

1. **Globals**: Hard to reason about data flow; any function can touch call state; testing is difficult.
2. **Single file**: Hard to navigate; merge conflicts; long compile times.
3. **Two different “DB” mechanisms**: (a) **Asterisk DB** – DBGet/DBPut/DBDel call Asterisk’s internal database via AGI (DATABASE GET/PUT/DEL); used for runtime call state, agent login, DYNLOGIN, etc. (b) **SQLite** – pbx3 tenant DB via shared handle + **`sqlQueryBind1`/`sqlQueryBind2`** and **`load_cluster_cfg`**. They are quite separate; treat them as distinct.
4. **Historical duplicate SQLite paths**: Addressed — single bind-based executor for runtime reads; `sqlQuery` / `DBQuery` / `sqlSelectEq` removed.
5. **Dead/optional code**: Commented cases (OutCos, OutCluster, Alias, hangUp, SetTimer 33–35), PlayGreet in a block comment; cfTab “get rid” note.
6. **Switch in main**: Addressed in Phase 2.1 — add a table row (+ handler); optional extract to `agi_commands.c` still open.

---

## Refactor Strategy: Incremental, Low-Risk

Goal: **simplify and modularise without big rewrites**. Prefer extraction and clear boundaries over a full rewrite.

---

### Phase 0: AGI test harness (**required**)

**Requirement:** Do not start Phase 1.1 (struct globals) or Phase 2+ file splits until Phase 0 acceptance criteria are met.

**Spec:** **`TEST_HARNESS.md`** (full detail). **Run:** **`TEST_RECIPE.md`** (`make test` on golden/Linux).

**Summary:**

| Piece | Approach |
|-------|----------|
| **Invoke** | Pipe AGI env block to stdin; argv as dialplan; one process per scenario |
| **Tenant data** | Fixture copy of `sqlite.rdonly.db` (golden export); env override for path |
| **AstDB** | Responder answers `DATABASE GET` from fixture `astdb.sqlite3` or key map — no Asterisk |
| **Channel** | Transcript assertions on stdout (`must` / `must-not`); benign `200` replies for `EXEC` |
| **First regressions** | CFIM local (no hold clip), CFIM external (comfort tones), empty forward |

**Outcome:** Repeatable offline tests for the class of bugs found in golden QA (e.g. CFCheck `strlen(cfnum)`). Refactor phases run the suite instead of relying on manual calls only.

---

### Phase 1: Consolidate and Clarify (no new files)

**1.1 Group globals into a small number of structs (call context)** — **done 2026-07-25**

- Added `agi_call_ctx_t` (`g_call`) and `agi_parms_t` (`g_parms`) in `pbx3cagi.h`.
- Call identity / cluster / locality flags and argv/`switchdig` live in those structs.
- Compatibility macros in `pbx3cagi.c` keep existing names (`callerid`, `myargv`, …) so handlers are unchanged for now.
- Left `rescols`, `abstimeout`/`abstimeint`, `debug`, CLID scratch, route-class scratch as plain globals (Phase 3 / later).
- Gate: `make test` PASS.

**1.2 Clarify and deduplicate the two data-access layers**

- **Asterisk DB** (DBGet, DBPut, DBDel): these are thin wrappers around AGITool_database_get/put/del – they talk to *Asterisk’s* internal database (realtime/AstDB), not SQLite. Keep them grouped together; optionally add a comment block “Asterisk DB (via AGI) – runtime state, agent login, etc.”
- **SQLite – done (this cycle):** Runtime reads use **`sqlQueryBind1` / `sqlQueryBind2`** with `?` binds; shared connection; **`sqlQuery` removed**. Dynamic **column names** (e.g. `queue1`…`queue6`, ivrmenu `optionN`/`alertN`) still use `snprintf(myQuery, …, "SELECT %s FROM … WHERE pkey=?", col)` + **`sqlQueryBind1(myQuery, key)`** — identifiers are not bindable; values are.
- When extracting a module: group “Asterisk DB: …” vs “pbx3 SQLite: bind helpers + `load_cluster_cfg`”.
- Outcome today: one bound-read path for tenant data + explicit cluster load; easier to move into `agi_sqlite.c` later.

**1.3 Remove or isolate dead code** — **done 2026-07-25**

- Removed unused `cfTab` (never read).
- Removed unreachable handlers: `hangUp`, `Page`, `SetTimer`, `AgentSpy` (no `switch` cases called them; star-code cases that used to dispatch them were already gone).
- Removed orphan prototypes from `pbx3cagi.h` (`Alias`, `miscSvcs`, `SetCluster`, `RingGroup`, `HuntGroup`, `OutGroup`, `Voicemail`, `EchoTest`, `DateTime`, `SysRestart`, `DialBack`, `GetDBProp`, `GetKeys`, `DBQueryKeys`, `SetOperator`, `SetCFExtrn`, `routeClass`, etc.).
- Commented OutCos/OutCluster/PlayGreet blocks were already absent from `main`.
- Gate: `make test` (7 scenarios) PASS after cleanup.

---

### Phase 2: Split by Domain (new source files, same binary)

**2.1 Command table instead of big switch** — **done 2026-07-25**

- Added `agi_cmd_entry_t` + static `agi_cmd_table[]` in `pbx3cagi.c` (still one TU; extract to `agi_commands.c` later if useful).
- Named cmds (`OutTrunk`…`PostDial`) and feature-code case numbers share one table; thin wrappers for handlers that need `PARM_*` (`cmd_OutTrunk`, `cmd_Dial`, `cmd_IVR`).
- `main()`: name → `switchdig` via `agi_cmd_lookup_name`, then `agi_cmd_dispatch(switchdig)`.
- Gate: `make test` PASS. Next: **2.2** SQLite extract, **2.3** init extract, or **Phase 3** pass pointers / drop macros.

**2.2 Extract SQLite module (pbx3 data only)** — **done 2026-07-25**

- New files: `agi_sqlite.c`, `agi_sqlite.h`. Makefile links `agi_sqlite.o`.
- Moved: shared handle + `sqlitedb_path` (`PBX3CAGI_SQLITE_DB` override), `sqlQueryBindInternal` / `sqlQueryBind1` / `sqlQueryBind2`, `load_cluster_cfg` (+ defaults / escape), `rescols`, `g_cluster_cfg`.
- Left in `pbx3cagi.c`: `pbx3_fleet_mode` (fleet dial logic that *calls* bind), Asterisk DB `DBGet`/`DBPut`/`DBDel`.
- Lock-retry still uses AGI `Wait` (same behaviour); module takes `extern` `agi`/`res`/`debug`/`vmsg` for that.
- Gate: `make test` PASS. Next: **2.3** init extract, or **Phase 3** pass pointers / drop macros.

**2.3 Extract “call context” initialisation**

- Move the block in `main()` that fills call identity from AGI vars (`callerid`, `extension`, `rdnis`, locality flags, etc.), resolves **tenant/cluster from dialplan argv** (`PARM_CLST` / `myargv[3]`, with fallback to `agi_context`), sets accountcode / `load_cluster_cfg`, etc. into one function, e.g. `agi_init_call_context(agi_call_ctx_t *ctx, int argc, char **argv)`.
- There is **no** `SetCluster()` anymore — cluster is passed in from `extensions.conf` (GenAst) as an AGI arg; this phase only extracts that inline setup.
- `main()` becomes: AGITool_Init → agi_init_call_context → setMoh → command table dispatch → AGITool_Destroy.
- Outcome: main() is a short, readable sequence; context setup is one place.

---

### Phase 3: Pass Context Explicitly (reduce globals)

**3.1 Pass a context pointer into handlers**

- Define a single “session” or “call” struct that holds:
  - agi_call_ctx_t (or equivalent),
  - agi/res (or pointers to AGI tools + result),
  - rescols (or pointer to DB result buffer),
  - debug, abstimeout, etc.
- Change handler signatures to `void OutTrunk(call_ctx_t *ctx, const char *key)` (and similarly for others).
- In main(), build one `call_ctx_t` and pass `&ctx` to each handler. Replace global reads in each handler with `ctx->...` in small steps.
- Outcome: no (or minimal) globals for call state; easier to test and to support multiple calls later if needed.

**3.2 Optional: AGI abstraction**

- Introduce a thin wrapper (e.g. `agi_session_t`) that holds `AGI_TOOLS*` and `AGI_CMD_RESULT*` and exposes “get variable”, “exec”, “set”, etc. Handlers take `call_ctx_t *ctx` and use `ctx->agi` or `ctx->session` for all AGI calls.
- Outcome: one place for AGI interaction; easier to mock in tests or swap implementation.

---

### Phase 4: Optional Simplifications

- **Split command handlers by domain**: e.g. `agi_cf.c` (CFToggle, CFVMail*, FollowMe, CFOff), `agi_agents.c` (Agent*, ChanSpy*), `agi_ivr.c` (IVR, IVRAction), `agi_outbound.c` (OutTrunk, OutVoip, OutRoute), `agi_inbound.c` (Inbound, InCall). Each file gets its own slice of the command table.
- **Replace magic numbers**: switchdig 18–23, 26–29, 63–68, etc. → named constants or an enum (e.g. CMD_CF_VMAIL_SET, CMD_AGENT_LOGIN).
- **Config**: Paths (SQLITEDB, SOUNDIR, QLOG) and maybe debug in a small struct or config module instead of macros/globals.

---

## Suggested Order of Work

**Priority note (2026-07-04):** Fleet **S8** and call recordings **R1 → S7** take precedence over pbx3cagi struct refactor. Phase 0 harness is **built**; run **`make test`** on golden; resume Phase 1.3+ when product work allows.

0. **Baseline:** Deploy **1.0.0-2**; golden manual QA; tag stable — **done**.
1. **Phase 0:** AGI test harness — **`TEST_HARNESS.md`** deliverables 0.1–0.8 — **built on `main`**; golden sign-off in progress. **Gate** for Phase 1.1+ when refactor resumes.
2. **Product (now):** **S8** fleet lifecycle → **R1** recordings management → **S7** recordings S3 — **`pbx3/pbx3-directory/docs/IMPLEMENTATION_PLAN.md`**, **`pbx3/workingdocs/TODO.md`**.
3. **Phase 1.2** (SQLite) – **largely complete:** bound reads (`sqlQueryBind1`/`2`), shared handle, removed `sqlQuery`/`DBQuery`/`sqlSelectEq`.
4. **Phase 1.3** (dead code) – **done 2026-07-25** (`make test` green).
5. **Phase 1.1** (struct for call context) – **done 2026-07-25** (`g_call` / `g_parms` + name macros; `make test` green).
6. **Phase 2.1** (command table) – **done 2026-07-25** (`agi_cmd_table` + dispatch; `make test` green).
7. **Phase 2.2** (DB module) – **done 2026-07-25** (`agi_sqlite.c`; `make test` green).
8. **Phase 2.3** (init_call_context). **Next** (or Phase 3).
9. **Phase 3** — pass context pointer; AGI abstraction aligns with harness mock layer.

---

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| Behaviour change | One logical change per commit; keep build + manual test after each step. |
| Regressions | **Phase 0 required:** scenario harness (`TEST_HARNESS.md`) — transcript + exit code; run after each refactor step. Manual golden calls for integration only. |
| Merge conflicts | Do Phase 1 and 2.1 first; smaller, focused files reduce conflict surface. |
| Performance | Avoid extra copying when introducing structs; pass pointers. DB layer change should not add heavy abstraction. |

---

## Summary

- **Immediate (product):** **S8** fleet ops → **R1** recordings management → **S7** recordings S3 — see **`pbx3/workingdocs/TODO.md`**.
- **Immediate (pbx3cagi):** Phase 0 harness **built**; golden **`make test`**; **defer** Phase 1.1+ struct refactor until fleet/recordings underway.
- **Short term (when refactor resumes):** Group globals into structs, tidy dead code, introduce a command table — run harness after each step.
- **Medium term**: Extract DB into `agi_sqlite.c`, and call context init into a single function; keep one binary.
- **Long term**: Pass a single context pointer into all handlers, then optionally split handlers by domain and introduce a small AGI abstraction.

This keeps the same process, same binary name, and same external behaviour while making the codebase easier to work on and test.

---

## Deferred TODO (pbx3cagi)

1. **Debian / dpkg packaging** — The compiled binary `pbx3cagi-1.0.0/csource/pbx3cagi` is **gitignored** (no longer committed). Package builds must run `make` in `pbx3cagi-1.0.0/csource` on the target platform (e.g. **Linux arm64**) and install the built artifact. Review and update `debian/rules` (or whatever copies the AGI into the package) so it does not rely on a pre-checked-in binary.

---

## For the next chat (handoff)

**What’s done**
- **Cleanup + SQLite refactor (pbx3cagi repo):** Build fixes, bsd_compat, `g_cluster_cfg` / `load_cluster_cfg`, redundant cluster re-queries removed, shared SQLite handle, **`sqlQueryBind1` / `sqlQueryBind2`** for all runtime reads, **`sqlQuery` removed**, `sqlSelectEq` removed (inline SQL), **`CheckState` `rescols` index fix**, compiled binary **gitignored**. Calls/SQL verified on target.
- **Phase 0 harness (2026-07-04):** On **`main`** — synthetic fixture, CFIM scenarios, **`make test`**, **`TEST_RECIPE.md`**. Golden validation ongoing.
- **Refactor plan:** This document — Asterisk DB vs pbx3 SQLite; Phase 1.2 SQLite path effectively done.

**What’s next**
1. **Product priority:** **S8** fleet → **R1** recordings management → **S7** S3 offload — **`pbx3/pbx3-directory/docs/IMPLEMENTATION_PLAN.md`**, **`pbx3/workingdocs/TODO.md`**.
2. **pbx3cagi:** Golden **`make test`** sign-off; **defer Phase 1.3+** until product work allows.
3. **When refactor resumes:** Phase 1.3 (dead code) → 1.1 (structs) → 2.x — run harness after each step.
4. **Packaging:** Pre-built amd64/arm64 in deb install tree; `debian/rules` runs `make` on target arch.

**Where things live**
- Plan: `pbx3cagi/workingdocs/REFACTOR_PLAN.md`
- Harness spec: `pbx3cagi/workingdocs/TEST_HARNESS.md`
- Handoff snapshot: `pbx3cagi/workingdocs/NEXT_AGENT_PBX3CAGI.md`
- Source: `pbx3cagi-1.0.0/csource/pbx3cagi.c` (~3210 lines), `pbx3cagi.h`, `cagi.c`, `cagi.h`, `bsd_compat.h`
- Grep: `sqlQueryBind`, `load_cluster_cfg`, `sqlGetSharedHandle` — results in `rescols[]`.
