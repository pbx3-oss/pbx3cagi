# ChanSpy lab proof (PRE_RELEASE_SAFETY_DEBT #3)

**Status:** Offline AGI scenarios green (`spy-default-denied`, `spy-ok`). **Physical desk proof still required** before item 3 is closed.

**Codes:** `*68*<ext>` listen · `*67*<ext>` whisper · tenant **Spy pass** (Advanced).

## Desk checklist (golden or bzy — single tenant)

1. SPA → Tenants → Advanced → set **Spy pass** to a non-default value (not `3333` / empty). Save + Commit if needed.
2. Two extensions on that tenant: **A** on a live call, **B** dials `*68*` + A’s extension (optionally `*67*` whisper).
3. Expect: password prompt → correct pass → ChanSpy attaches; wrong pass → deny.
4. Set Spy pass back to `3333` (or clear) → dial again → **must deny** (no attach). Restore a real pass after.
5. Note date, node FQDN, tenant shortuid here when green:

| Date | Node | Tenant | `*68*` | `*67*` | Default denied | Tester |
|------|------|--------|--------|--------|----------------|--------|
| | | | | | | |

Multi-tenant ChanSpy remains inventory **H** debt — do not claim fleet-wide spy green from this single-tenant proof.
