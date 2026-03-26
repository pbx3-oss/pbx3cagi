# NEXT AGENT: pbx3cagi (dbstruct)

## Current baseline
- Branch: `dbstruct`
- Status: `pbx3cagi` builds and basic inbound/outbound call handling was verified on target after the latest changes.

## What was done (high level)
1. **`pbx3cagi` runtime streamlining**
   - Removed redundant local copies from `g_cluster_cfg` in these call paths:
     `OutVoip`, `OutRoute`, `OutTrunk`, `LepDial`, `CFCheck`, `Ingress`, `IVR`.
   - Kept only the locals that are required for buffers assembled from multiple sources (e.g. `intRingDelay`, CLID overrides).
2. **SQLite handling consolidation**
   - Eliminated the second sqlite execution path by routing the legacy `DBQuery`-style helper calls through the single executor (`sqlQuery`), via a small selector wrapper (`sqlSelectEq`).
   - Result: fewer duplicated sqlite logic branches and a clearer single “SQLite read” implementation path.
3. **Docs alignment**
   - `REFACTOR_PLAN.md` was moved under `pbx3cagi/workingdocs/` so it’s closer to the code-focused handoff work.

## New contract: canonical data for fast literal comparisons
- `pbx3cagi` uses **literal sentinel strings** for states (e.g. `"enabled"`, `"YES"`, `"None"`).
- SQLite `NULL` is **unknown** (tristate) in SQL semantics; in `sqlQuery()` `NULL` becomes an empty string (`""`) in `rescols[]`.
- Therefore the side project is to ensure the migrated/new DB is **constraint/canonicalization compliant** so those comparisons never see “unexpected absence” representations.

### Side project reference (outside this repo)
- See: `pbx3/workingdocs/SQL_CHECK_CONSTRAINT_SIDEPROJECT.md`

## What to do next
1. **Re-review pbx3cagi with the assumption of compliance**
   - If the DB guarantees `NOT NULL` + allowed literals for sentinel columns, we can simplify:
     - `load_cluster_cfg()` NULL branching
     - any defensive logic that exists only because legacy rows could contain `NULL`/empty/variant spellings
2. **Add any remaining migrations/constraints**
   - Implement `NOT NULL` and `CHECK (...)` in the *new* schema (safe because loader/migration is the only producer of the new DB).
3. **Run a focused regression subset**
   - CF external forward (`cfwd_progress`, `cfwd_answer`)
   - route/trunk fail tone flags (`play_*`)
   - IVR timing (`ivr_key_wait`, `ivr_digit_wait`)
   - ingress ring + `lterm`

