# NEXT AGENT: pbx3cagi

## Current baseline
- Branch: **`main`**
- Package: **1.0.0-2** (CFCheck fix, amd64 + arm64 binaries in deb install tree)
- Status: Golden **08jzwn** QA passed for CoS, CFIM, runtime/AstDB shortuid, GenAst, local CFIM divert audio.

## What was done (recent)
- **CFCheck:** `strlen(cfnum)` for local vs external comfort tone (was `strlen(number)` on shortuid key).
- **Golden QA merged** to `main` (pbx3, pbx3api, pbx3cagi); `goldenQA` branch deleted.
- **Refactor plan:** Phase 0 AGI test harness documented as **required gate** before Phase 1.1+.

## What to do next
1. **Phase 0 (required):** Implement AGI test harness per **`TEST_HARNESS.md`** — fixture tenant DB, AstDB protocol mock, transcript scenarios (CFIM local/external first). **Do not start Phase 1.1 struct refactor until Phase 0 passes.**
2. **Phase 1.3** (dead code) after Phase 0.
3. **Schema side project:** `pbx3/workingdocs/SQL_CHECK_CONSTRAINT_SIDEPROJECT.md` — simplify `load_cluster_cfg` NULL branching when DB is canonical.

## Docs
- **`REFACTOR_PLAN.md`** — phases and order of work
- **`TEST_HARNESS.md`** — Phase 0 deliverables 0.1–0.8
- Source: `pbx3cagi-1.0.0/csource/pbx3cagi.c`, `cagi.c`, `pbx3cagi.h`
