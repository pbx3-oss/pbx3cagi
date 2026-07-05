# NEXT AGENT: pbx3cagi

## Current baseline
- Branch: **`main`**
- Package: **1.0.0-2** (CFCheck fix, amd64 + arm64 binaries in deb install tree)
- Status: Golden **08jzwn** — live call QA + **Phase 0 `make test` signed off** (seed + live tenant DB).

## What was done (recent)
- **Phase 0 harness:** Synthetic fixture, CFIM scenarios, `make test`, **`TEST_RECIPE.md`** — on **`main`**.
- **CFCheck:** `strlen(cfnum)` for local vs external comfort tone.
- **Golden QA merged** to `main` (pbx3, pbx3api, pbx3cagi).

## Product priority (2026-07-04)
**pbx3cagi struct refactor deferred.** Fleet + recordings first — see **`pbx3/workingdocs/TODO.md`**, **`pbx3/pbx3-directory/docs/IMPLEMENTATION_PLAN.md`**.

1. **S8** — fleet checklist, IAM/`.env` hardening, tenant migration.
2. **R1** — call recordings management (API + SPA; local disk).
3. **S7** — recordings S3 offload.

## What to do next (pbx3cagi repo)
1. **Product priority:** **S8** → **R1** → **S7** — see **`pbx3/workingdocs/TODO.md`**.
2. **When product allows:** Phase **1.3** (dead code) → **1.1** (structs) — run **`make test`** after each commit.
3. **Recording capture:** likely minimal cagi changes for R1; capture already via SetRecord.

## Docs
- **`REFACTOR_PLAN.md`** — phases and order of work
- **`TEST_HARNESS.md`** · **`TEST_RECIPE.md`** — Phase 0 spec and runbook
- Source: `pbx3cagi-1.0.0/csource/pbx3cagi.c`, `cagi.c`, `pbx3cagi.h`
