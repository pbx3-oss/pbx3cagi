# NEXT AGENT: pbx3cagi (dbstruct)

## Current baseline
- Branch: `dbstruct`
- Status: `pbx3cagi` builds; SQL retrievals and calls verified on target (Linux arm64). Compiled binary is **gitignored** — build with `make` in `pbx3cagi-1.0.0/csource` on the target arch.

## What was done (high level)
1. **`pbx3cagi` runtime streamlining**
   - Removed redundant local copies from `g_cluster_cfg` in these call paths:
     `OutVoip`, `OutRoute`, `OutTrunk`, `LepDial`, `CFCheck`, `Ingress`, `IVR`.
   - Kept only the locals that are required for buffers assembled from multiple sources (e.g. `intRingDelay`, CLID overrides).
2. **SQLite**
   - **Single shared connection** for the AGI process (`sqlGetSharedHandle`, closed via `atexit` on normal exit).
   - **Tenant reads:** `sqlQueryBind1` / `sqlQueryBind2` (internal `sqlQueryBindInternal`) — all runtime `SELECT`s use `?` placeholders; results in `rescols[]`. Legacy **`sqlQuery` removed** (no call sites).
   - **`load_cluster_cfg`:** still uses direct `sqlite3_*` for the one cluster row (escaped `pkey`/`shortuid`); extra cluster fields loaded once into `g_cluster_cfg` (no duplicate cluster `SELECT`s on hot paths).
   - **Inline SQL:** `sqlSelectEq` removed; explicit `SELECT` strings at call sites where helpful.
   - **Fix:** `CheckState` — `inroutes` row uses `rescols[0..2]` for `cluster`, `openroute`, `closeroute` (was wrongly using `[3]`/`[4]`).
3. **Docs alignment**
   - `REFACTOR_PLAN.md` lives under `pbx3cagi/workingdocs/`.

## New contract: canonical data for fast literal comparisons
- `pbx3cagi` uses **literal sentinel strings** for states (e.g. `"enabled"`, `"YES"`, `"None"`).
- SQLite `NULL` is **unknown** (tristate); bound-query path maps NULL text columns to empty string (`""`) in `rescols[]` (same idea as before).
- Side project: ensure migrated/new DB is **constraint/canonicalization compliant** so comparisons never see unexpected absence.

### Side project reference (outside this repo)
- See: `pbx3/workingdocs/SQL_CHECK_CONSTRAINT_SIDEPROJECT.md`

## What to do next
1. **Re-review pbx3cagi with the assumption of compliance**
   - If the DB guarantees `NOT NULL` + allowed literals for sentinel columns, simplify `load_cluster_cfg()` NULL branching and other defensive logic.
2. **Debian packaging** — see **Deferred TODO** in `REFACTOR_PLAN.md` (binary gitignored; `debian/rules` must run `make`).
3. **Run a focused regression subset**
   - CF external forward (`cfwd_progress`, `cfwd_answer`)
   - route/trunk fail tone flags (`play_*`)
   - IVR timing (`ivr_key_wait`, `ivr_digit_wait`)
   - ingress ring + `lterm` + **CheckState / open–closed routing**
