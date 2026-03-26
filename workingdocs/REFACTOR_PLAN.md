# pbx3cagi Refactor Plan

## Agreed route forward

1. **Ship the cleanup first.** The changes already made (compile fixes, braces, typos, strlcpy, semicolon, pkey→key, sign-compare casts, bsd_compat, header prototypes) are committed. Deploy that build, get it working in the target environment, and complete the required testing. Establish a **stable, known-good baseline**.
2. **Refactor in phases; test after each phase.** Do not do one big refactor-and-test. Do e.g. Phase 1.2 (unify on sqlQuery), test; then Phase 1.3 (dead code), test; then next phase. Each phase is a small, testable step. If something breaks, the last phase is the suspect.
3. **Fallback.** Always have a recent known-good state (tag or branch) to revert to.

---

## Current State

- **pbx3cagi.c**: ~3,325 lines, single translation unit.
- **~35 global variables** (call state, cluster, channel, DB result buffer, AGI args, debug).
- **~45 functions**: one big `main()` with a 30+ case switch, then command handlers and shared helpers.
- **Tight coupling**: every command handler reads/writes globals (`agi`, `res`, `myCluster`, `rescols[]`, `callerid`, etc.) and calls shared DB/AGI helpers.

### Data access: two separate systems

- **Asterisk DB** (DBGet, DBPut, DBDel): Calls into *Asterisk’s* internal database via AGI (DATABASE GET/PUT/DEL). Used for runtime call state, agent login (e.g. DYNLOGIN, eAgent, dAgent), and similar. No SQLite; no pbx3 schema.
- **SQLite** (DBQuery, sqlQuery): Opens the *pbx3* tenant/instance SQLite DB in-process (e.g. `/opt/pbx3/db/sqlite.rdonly.db`). Used for cluster, trunks, ipphone, agent table, ivrmenu, etc. Quite separate from Asterisk DB. Historically **sqlQuery succeeded DBQuery** in the old system; it makes sense to go forward with **only one call type (sqlQuery)**. All SQLite access should use sqlQuery; DBQuery should be phased out (call sites migrated to build the query string and call sqlQuery, then DBQuery removed).

Refactor steps should treat these as distinct: e.g. “SQLite module” = pbx3 data only; “Asterisk DB” stays as AGI wrappers. SQLite API = single entry point: sqlQuery.

### Main structural elements

| Section              | Approx lines | Role |
|----------------------|-------------|------|
| Globals + cfTab      | 1–80        | Call context, AGI args, constants |
| main()               | 116–399     | Init, SetCluster, switch(switchdig) → handlers |
| SetCluster / setMoh  | 406–560     | Cluster resolution, MOH |
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
| **SQLite** (DBQuery, sqlQuery) | 2915–3110 | pbx3 tenant/instance data; sqlQuery is the successor to DBQuery – unify on sqlQuery only |
| OutQmt, QLogWrite, outboundClip, consoleMsg | 3133–3325 | Queue log, clip, logging |

---

## Pain Points

1. **Globals**: Hard to reason about data flow; any function can touch call state; testing is difficult.
2. **Single file**: Hard to navigate; merge conflicts; long compile times.
3. **Two different “DB” mechanisms**: (a) **Asterisk DB** – DBGet/DBPut/DBDel call Asterisk’s internal database via AGI (DATABASE GET/PUT/DEL); used for runtime call state, agent login, DYNLOGIN, etc. (b) **SQLite** – DBQuery/sqlQuery open the pbx3 tenant/instance SQLite DB in-process for cluster, trunks, ipphone, and other config. They are quite separate; the refactor plan should treat them as distinct.
4. **Duplicate patterns**: DBQuery and sqlQuery share almost identical execution logic (open, prepare with retry, step, fill rescols). Unifying on sqlQuery only removes duplication and one code path.
5. **Dead/optional code**: Commented cases (OutCos, OutCluster, Alias, hangUp, SetTimer 33–35), PlayGreet in a block comment; cfTab “get rid” note.
6. **Switch in main**: Adding a command means editing main() and a large file.

---

## Refactor Strategy: Incremental, Low-Risk

Goal: **simplify and modularise without big rewrites**. Prefer extraction and clear boundaries over a full rewrite.

---

### Phase 1: Consolidate and Clarify (no new files)

**1.1 Group globals into a small number of structs (call context)**

- Introduce 2–3 structs in the header, e.g.:
  - `agi_call_ctx_t`: callerid, extension, channel, context, rdnis, uniqueid, myCluster, myClusterContext, myClusterId, chanId.
  - `agi_parms_t`: myargv, myargc, switchdig (or equivalent).
  - Keep `rescols` and maybe `abstimeout`/`abstimeint` in a “runtime” or “db_result” struct if you want, or leave as globals for now.
- Replace individual global reads/writes with `ctx->callerid`, etc., **one variable at a time**, with a single commit per rename so behaviour is unchanged.
- Outcome: one place that defines “call context”; easier to pass a pointer later.

**1.2 Clarify and deduplicate the two data-access layers**

- **Asterisk DB** (DBGet, DBPut, DBDel): these are thin wrappers around AGITool_database_get/put/del – they talk to *Asterisk’s* internal database (realtime/AstDB), not SQLite. Keep them grouped together; optionally add a comment block “Asterisk DB (via AGI) – runtime state, agent login, etc.”
- **SQLite – unify on sqlQuery only**: sqlQuery succeeded DBQuery in the old system; go forward with a single call type. (1) Migrate every `DBQuery(table, wherecol, whereval, column)` call site to build the SELECT query string (e.g. `snprintf(myQuery, sizeof(myQuery), "SELECT %s FROM %s WHERE %s='%s'", ...)`) and call `sqlQuery(myQuery)`; result still in `rescols[]`. (2) Remove `DBQuery()`. (3) Keep one contiguous “pbx3 SQLite” block containing only `sqlQuery` (and optionally one internal `static` helper for open/prepare/step/fill/close if you want to tidy the function).
- Add a short comment at the top of each block: “Asterisk DB: …” vs “pbx3 SQLite: sqlQuery only”. Do not conflate the two when extracting modules later.
- Outcome: two clear boundaries (Asterisk DB vs SQLite); one SQLite entry point (sqlQuery); no duplicate execution logic; easier to extract SQLite to its own module later.

**1.3 Remove or isolate dead code**

- Either delete commented-out cases (OutCos, OutCluster, Alias, hangUp, SetTimer 33–35) and the commented PlayGreet block, or move them to a single “#if 0 … #endif” block at the bottom with a “Legacy/unused” comment.
- Resolve cfTab: either trim to used indices and add a comment, or replace the table with a small function (switchdig → string). Document the mapping.
- Outcome: less noise and fewer “is this used?” questions.

---

### Phase 2: Split by Domain (new source files, same binary)

**2.1 Command table instead of big switch**

- Define a small struct, e.g. `{ int case_num; void (*handler)(void); }` or `handler(int argc, char **argv)`.
- Fill a static table: case 1 → OutTrunk, case 2 → OutRoute, … .
- In main(), after setting switchdig, loop over the table and call the matching handler (or use a map if you prefer).
- Move the table and handler declarations to a new file, e.g. `agi_commands.c` / `agi_commands.h`, with handlers still implemented in pbx3cagi.c (or move one handler at a time later).
- Outcome: adding a command = adding a row and implementing a function; main() stays small.

**2.2 Extract SQLite module (pbx3 data only) – sqlQuery only**

- New files: e.g. `agi_sqlite.c`, `agi_sqlite.h` (or `pbx3_db.c`), to make clear this is *pbx3* data, not Asterisk DB.
- Move **sqlQuery** (and its execution logic) into this module. Expose a single API, e.g. `sqlQuery(char *query)` or `agi_sqlite_query(char *query)`, with results in a `rescols`-like interface (or a struct passed in). There is no DBQuery in the new module – callers build the query string and call sqlQuery (Phase 1.2 will have already migrated away from DBQuery).
- **Do not** move DBGet/DBPut/DBDel into this module – they are Asterisk DB (AGI DATABASE GET/PUT/DEL), not SQLite. Keep them in pbx3cagi.c (or later in a small “asterisk_db” wrapper that takes agi/res and calls AGITool_database_*).
- pbx3cagi.c then uses the new SQLite module (single call type: sqlQuery) for cluster/trunks/ipphone etc., and continues to call DBGet/DBPut/DBDel for Asterisk state.
- Outcome: pbx3 SQLite access lives in one place with one call type (sqlQuery); Asterisk DB remains AGI-side and separate.

**2.3 Extract “call context” initialisation**

- Move the block that sets callerid, extension, cluster, rdnis_is_local, etc. (everything up to and including SetCluster()) into one function, e.g. `agi_init_call_context(agi_call_ctx_t *ctx, int argc, char **argv)`.
- main() becomes: AGITool_Init → agi_init_call_context → setMoh → command table dispatch → AGITool_Destroy.
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

0. **Before refactor:** Deploy current cleanup build; run integration and manual tests; fix any issues; tag a stable baseline.
1. **Phase 1.2** (unify SQLite on sqlQuery: migrate DBQuery call sites → sqlQuery, remove DBQuery) – low risk, one call type, immediate clarity. Test after.
2. **Phase 1.3** (dead code) – quick cleanup. Test after.
3. **Phase 1.1** (struct for call context) – one global at a time.
4. **Phase 2.1** (command table) – then you can add commands without touching the big switch.
5. **Phase 2.2** (DB module) – move SQLite behind agi_db.c.
6. **Phase 2.3** (init_call_context).
7. **Phase 3** when you’re ready to reduce globals and improve testability.

---

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| Behaviour change | One logical change per commit; keep build + manual test after each step. |
| Regressions | Add a small “smoke” script that runs pbx3cagi with a few argv patterns and checks exit code (and maybe AGI output) if possible. |
| Merge conflicts | Do Phase 1 and 2.1 first; smaller, focused files reduce conflict surface. |
| Performance | Avoid extra copying when introducing structs; pass pointers. DB layer change should not add heavy abstraction. |

---

## Summary

- **Short term**: Group globals into structs, tidy the DB block and remove duplication, remove or isolate dead code, introduce a command table.
- **Medium term**: Extract DB into `agi_db.c`, and call context init into a single function; keep one binary.
- **Long term**: Pass a single context pointer into all handlers, then optionally split handlers by domain and introduce a small AGI abstraction.

This keeps the same process, same binary name, and same external behaviour while making the codebase easier to work on and test.

---

## For the next chat (handoff)

**What’s done**
- **Cleanup (committed in pbx3cagi repo):** Build fixes (ctype.h, brace/typo/syntax, path\[last\], strlcpy, semicolon, OutVoip(key), sign-compare casts), bsd_compat strlcpy/strlcat fallback, pbx3cagi.h prototype updates. Build is clean (zero warnings).
- **Refactor plan:** This document (REFACTOR_PLAN.md) with phased plan, Asterisk DB vs SQLite clarified, “unify on sqlQuery only” and “agreed route forward” (ship cleanup → test → refactor in phases with test after each).

**What’s next**
1. **Before any refactor:** Deploy the current pbx3cagi binary, run integration/staging and manual tests, fix any environment issues. Get to a stable baseline and (if possible) tag it.
2. **First refactor phase when ready:** Phase 1.2 – unify SQLite on sqlQuery (migrate all DBQuery call sites to build query + sqlQuery(myQuery), remove DBQuery). Then test again.

**Where things live**
- Plan: `pbx3cagi/workingdocs/REFACTOR_PLAN.md`
- Source: `pbx3cagi-1.0.0/csource/pbx3cagi.c` (~3325 lines), `pbx3cagi.h`, `cagi.c`, `cagi.h`, `bsd_compat.h`
- DBQuery call sites: ~45 uses of `DBQuery(...)` in pbx3cagi.c; grep for `DBQuery(` to find them. sqlQuery takes a pre-built query string; results still in `rescols[]`.
