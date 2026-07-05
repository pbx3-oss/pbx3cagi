# PBX3AGI Working Docs

This folder contains documentation for AI agents working on the pbx3agi codebase.

## Contents

| File | Purpose |
|------|---------|
| **REFACTOR_PLAN.md** | Incremental refactor phases; **Phase 0 harness built**; struct refactor deferred until S8 + R1 |
| **TEST_RECIPE.md** | **Copy-paste runbook** — `make test` on Linux/golden (start here when you forget) |
| **TEST_HARNESS.md** | **Phase 0 spec** — offline AGI scenarios, fixtures, acceptance criteria |
| **CODE_ASSESSMENT.md** | Overall code review: strengths, issues, architecture observations, recommendations |
| **REWRITE_ANALYSIS.md** | Pros/cons of rewriting, language recommendations, migration strategies |
| **INCREMENTAL_IMPROVEMENT_PLAN.md** | Step-by-step plan to improve C code incrementally with testable steps |

## For New AI Sessions

When starting work on pbx3agi:

1. **Read REFACTOR_PLAN.md** — Phase 0 test harness is **required** before Phase 1.1+ refactor
2. **Read TEST_RECIPE.md** — run offline scenarios on golden/Linux (`make test`)
3. **Read TEST_HARNESS.md** — if implementing or extending offline AGI scenarios
4. **Read CODE_ASSESSMENT.md** - Understand current state, critical issues, architecture
5. **Read INCREMENTAL_IMPROVEMENT_PLAN.md** - Step-by-step plan for improving the code
6. **Read REWRITE_ANALYSIS.md** - If considering rewrite, see language options and migration strategies
7. **Check git history** - See what's been changed recently
8. **Review issues** - SQL injection and string safety are critical priorities

## Key Issues to Address

- 🔴 **SQL Injection:** All SQL queries need parameterization
- 🟡 **String Safety:** Replace unsafe `strcpy`/`strcat`/`sprintf` functions
- 🟡 **Code Organization:** Monolithic 3,400-line file needs modularization
- 🟢 **Error Handling:** Inconsistent error handling patterns

## Repository Structure

```
pbx3agi/
├── pbx3cagi-1.0.0/
│   ├── csource/          # Source code
│   │   ├── pbx3cagi.c    # Main application (~3,400 lines)
│   │   ├── pbx3cagi.h    # Header file
│   │   ├── cagi.c/h      # AGI library wrapper
│   │   ├── sqlite3.c/h   # Embedded SQLite
│   │   └── Makefile      # Build configuration
│   ├── build/            # Debian package build artifacts
│   └── images/           # Pre-built binaries (amd64, armel, i386)
└── workingdocs/          # This folder
```

## Context

**pbx3cagi** is a C-based Asterisk AGI application that handles:
- Call routing (outbound trunks, routes, inbound)
- IVR menus
- ACD queues and ring groups
- Call forwarding (CFIM, CFBS, CFVMail)
- Voicemail integration
- Agent management
- Call recording
- Multi-tenant/cluster support

The code reads from SQLite database (`/opt/pbx3/db/sqlite.rdonly.db`) and executes Asterisk commands via AGI protocol.

---

*Last updated: 2026-02-09*

## Current Work Plan

**Status:** Incremental C improvements (see INCREMENTAL_IMPROVEMENT_PLAN.md)

**Priority:** Phase 1 (SQL Injection fixes) → Phase 2 (String Safety) → Phase 3+ (Organization/Quality)

**Next Steps:** Start with Step 1.1 - Create Parameterized Query Helper Function
