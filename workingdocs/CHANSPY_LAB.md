# ChanSpy lab proof (PRE_RELEASE_SAFETY_DEBT #3)

**Status:** Offline AGI scenarios green (`spy-default-denied`, `spy-ok`, `spy-cross-tenant-denied`). **Physical desk green on golden 2026-08-09** (dhbm8x `*68*1000` from 1002 while 1000↔1001; shortuid ChanSpy fix `ecfc1a7`). **Cross-tenant deny:** tip **1.0.0-22** (drop pkey-only fallback) — desk re-prove on golden after install.

**Codes:** `*68*<ext>` listen · `*67*<ext>` whisper · tenant **Spy pass** (Advanced). Stock `3333` refused.

## Desk checklist (golden or bzy — single tenant)

1. SPA → Tenants → Advanced → set **Spy pass** to a non-default value (not `3333` / empty). Save + Commit if needed.
2. Two extensions on that tenant: **A** on a live call, **B** dials `*68*` + A’s extension (optionally `*67*` whisper).
3. Expect: password prompt → correct pass → ChanSpy attaches; wrong pass / empty → deny.
4. Set Spy pass back to `3333` (or clear) → dial again → **must deny** (no attach). Restore a real pass after.
5. Lab note:

| Date | Node | Tenant | `*68*` | `*67*` | Default denied | Tester |
|------|------|--------|--------|--------|----------------|--------|
| 2026-08-09 | golden / 08jzwn | dhbm8x | OK (1002→1000) | OK | stock refused by AGI | operator |

## Multi-tenant isolation (TODO 0n)

Target pkey is resolved **only** in the calling tenant (`cluster` = dialplan context). Same extension number on another tenant must **not** attach (`pbx-invalid` after spy pass).

| Date | Node | From | Dial | Expect | Result |
|------|------|------|------|--------|--------|
| 2026-09-26 | golden | Aelintra / hf3zzv `403` | `*67*1000` (Duns `fkdd5d`) | deny / no `ChanSpy PJSIP/fkdd5d` | **pre-fix red** — pkey-only fallback; fixed in **1.0.0-22** |
| 2026-09-26 | golden | Aelintra → Duns `1000` | `*67*1000` / `*68*1000` | `pbx-invalid`; no ChanSpy | **green** (tip AGI; deny cross-tenant) |
