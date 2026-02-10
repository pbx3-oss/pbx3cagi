# PBX3AGI Incremental Improvement Plan

**Date:** 2026-02-09  
**Approach:** Incremental C refactoring with testable steps  
**Goal:** Fix critical security issues and improve code quality without breaking production

---

## Principles

1. **Each step compiles** - No broken builds
2. **Each step is testable** - Can verify behavior on running system
3. **Incremental** - Small, safe changes
4. **Backward compatible** - Don't break existing functionality
5. **Test after each step** - Verify calls still work

---

## Phase 1: Critical Security Fixes

### Step 1.1: Create Parameterized Query Helper Function ✅

**Goal:** Add a safe `DBQueryParam` function that uses parameterized queries.

**Changes:**
- Add new function `DBQueryParam()` in `pbx3cagi.c` that uses `sqlite3_bind_*`
- Keep existing `DBQuery()` for now (backward compatibility)
- Test: Compile, verify new function works with test query

**Test:**
```bash
# Compile
cd csource && make clean && make

# Test new function (add test code temporarily)
# Verify it returns correct results
```

**Files:** `pbx3cagi.c`, `pbx3cagi.h`

---

### Step 1.2: Migrate One DBQuery Call to DBQueryParam

**Goal:** Replace one `DBQuery()` call with `DBQueryParam()` to validate approach.

**Changes:**
- Pick a simple, low-risk query (e.g., line 150: `DBQuery("master_xref", "pkey", callerid, "relation")`)
- Replace with `DBQueryParam()` call
- Test: Make a call, verify behavior unchanged

**Test:**
```bash
# Make test call through system
# Verify call routing works correctly
# Check logs for errors
```

**Files:** `pbx3cagi.c`

---

### Step 1.3: Migrate All DBQuery Calls (Batch 1 - Low Risk)

**Goal:** Replace all `DBQuery()` calls in `main()` function with `DBQueryParam()`.

**Changes:**
- Lines 150, 162, 172, 181: Replace `DBQuery()` calls
- Test after each replacement
- Keep old `DBQuery()` function for now

**Test:**
```bash
# Test inbound call routing
# Test outbound call routing
# Verify cluster detection works
# Check logs for SQL errors
```

**Files:** `pbx3cagi.c`

---

### Step 1.4: Migrate All DBQuery Calls (Batch 2 - Medium Risk)

**Goal:** Replace remaining `DBQuery()` calls throughout codebase.

**Changes:**
- Find all remaining `DBQuery()` calls (grep for pattern)
- Replace one function at a time (e.g., `SetCluster()`, then `outboundClip()`, etc.)
- Test after each function migration

**Test:**
```bash
# Test each feature:
# - Call forwarding
# - Voicemail
# - IVR menus
# - Queue operations
```

**Files:** `pbx3cagi.c`

---

### Step 1.5: Migrate sqlQuery() to Parameterized Version

**Goal:** Replace `sqlQuery()` with parameterized version.

**Changes:**
- Create `sqlQueryParam()` that accepts format string + variadic args
- Use `sqlite3_bind_*` for all parameters
- Migrate all `sqlQuery()` calls gradually

**Test:**
```bash
# Test complex queries (IVR, routing)
# Verify all features still work
```

**Files:** `pbx3cagi.c`, `pbx3cagi.h`

---

### Step 1.6: Remove Old Unsafe Query Functions

**Goal:** Remove `DBQuery()` and old `sqlQuery()` once all calls migrated.

**Changes:**
- Remove `DBQuery()` function
- Remove old `sqlQuery()` function
- Verify no compilation errors

**Test:**
```bash
# Full system test:
# - Inbound calls
# - Outbound calls
# - IVR
# - Queues
# - Call forwarding
# - Voicemail
```

**Files:** `pbx3cagi.c`, `pbx3cagi.h`

---

## Phase 2: String Safety

### Step 2.1: Replace sprintf() in Debug Functions

**Goal:** Replace `sprintf()` with `snprintf()` in debug functions.

**Changes:**
- Line 99: `DebugFunctionTrace()` - replace `sprintf()` → `snprintf()`
- Line 110: `DebugFunctionMsg()` - replace `sprintf()` → `snprintf()`
- Test: Enable debug mode, verify logging works

**Test:**
```bash
# Set DEBUG=ON in Asterisk
# Make test call
# Verify debug output appears correctly
```

**Files:** `pbx3cagi.c`

---

### Step 2.2: Replace strcpy() in main() Function

**Goal:** Replace `strcpy()` calls in `main()` with `strlcpy()`.

**Changes:**
- Line 151: `strcpy(relation, rescols[0])` → `strlcpy(relation, rescols[0], sizeof(relation))`
- Line 163: Same replacement
- Test: Make calls, verify routing works

**Test:**
```bash
# Test call routing
# Verify caller/callee detection works
```

**Files:** `pbx3cagi.c`

---

### Step 2.3: Replace strcpy() in DBQuery Functions

**Goal:** Replace `strcpy()` in database query result handling.

**Changes:**
- Line 3073: `strcpy(rescols[i], "")` → `strlcpy(rescols[i], "", sizeof(rescols[i]))`
- Line 3172: Same replacement
- Test: Test database queries, verify empty results handled correctly

**Test:**
```bash
# Test queries that return empty results
# Verify no crashes or corruption
```

**Files:** `pbx3cagi.c`

---

### Step 2.4: Replace strcat() Calls (Batch 1)

**Goal:** Replace `strcat()` calls with `strlcat()` in voicemail functions.

**Changes:**
- Lines 1571, 1607, 1687, 1750: Replace `strcat(vmflags, ...)` → `strlcat(vmflags, ..., sizeof(vmflags))`
- Test: Test voicemail operations

**Test:**
```bash
# Test voicemail:
# - Leave message
# - Retrieve message
# - Forward to voicemail
```

**Files:** `pbx3cagi.c`

---

### Step 2.5: Replace strcat() Calls (Batch 2)

**Goal:** Replace remaining `strcat()` calls throughout codebase.

**Changes:**
- Find all remaining `strcat()` calls (grep)
- Replace one function at a time
- Test after each replacement

**Test:**
```bash
# Test affected features after each change
```

**Files:** `pbx3cagi.c`

---

### Step 2.6: Audit All String Operations

**Goal:** Verify all string operations use safe functions.

**Changes:**
- Grep for `strcpy`, `strcat`, `sprintf`, `gets`
- Verify all replaced
- Add comments where safe functions used

**Test:**
```bash
# Full system test
# Static analysis (if available)
```

**Files:** `pbx3cagi.c`

---

## Phase 3: Error Handling

### Step 3.1: Standardize SQLite Error Handling

**Goal:** Create consistent error handling for SQLite operations.

**Changes:**
- Create helper function `handleSqliteError()` that logs and returns consistent codes
- Update `DBQueryParam()` to use it
- Test: Test database errors (lock, missing DB, etc.)

**Test:**
```bash
# Simulate database errors:
# - Lock database (from another process)
# - Remove database file temporarily
# - Verify graceful handling
```

**Files:** `pbx3cagi.c`, `pbx3cagi.h`

---

### Step 3.2: Add Return Value Checking

**Goal:** Check return values for critical operations.

**Changes:**
- Add checks for `AGITool_*` function returns
- Add checks for file operations (`fopen`, `fwrite`)
- Test: Verify error cases handled gracefully

**Test:**
```bash
# Test error scenarios:
# - Disk full (file writes)
# - Asterisk communication errors
```

**Files:** `pbx3cagi.c`

---

### Step 3.3: Improve Error Messages

**Goal:** Make error messages more informative.

**Changes:**
- Add context to error messages (function name, parameters)
- Include SQLite error codes in messages
- Test: Trigger errors, verify messages helpful

**Test:**
```bash
# Review error logs
# Verify errors are actionable
```

**Files:** `pbx3cagi.c`

---

## Phase 4: Code Organization

### Step 4.1: Extract Database Functions to Separate File

**Goal:** Move database functions to `db.c` / `db.h`.

**Changes:**
- Create `db.h` with function declarations
- Create `db.c` with `DBQueryParam()`, `sqlQueryParam()`, etc.
- Update `pbx3cagi.c` to include `db.h`
- Update Makefile to compile `db.c`
- Test: Compile, verify functionality unchanged

**Test:**
```bash
# Compile
make clean && make

# Test all database operations
```

**Files:** `db.c`, `db.h`, `pbx3cagi.c`, `Makefile`

---

### Step 4.2: Extract Routing Functions

**Goal:** Move routing functions to `routing.c` / `routing.h`.

**Changes:**
- Create `routing.h` with declarations
- Create `routing.c` with `OutTrunk()`, `OutRoute()`, `OutVoip()`, `InCall()`, `Inbound()`
- Update Makefile
- Test: Test all routing functionality

**Test:**
```bash
# Test:
# - Outbound trunk routing
# - Outbound route selection
# - Inbound call handling
```

**Files:** `routing.c`, `routing.h`, `pbx3cagi.c`, `Makefile`

---

### Step 4.3: Extract IVR Functions

**Goal:** Move IVR functions to `ivr.c` / `ivr.h`.

**Changes:**
- Create `ivr.h`, `ivr.c` with `IVR()`, `IVRAction()`
- Update Makefile
- Test: Test IVR menus

**Test:**
```bash
# Test IVR:
# - Menu navigation
# - Key presses
# - Timeouts
```

**Files:** `ivr.c`, `ivr.h`, `pbx3cagi.c`, `Makefile`

---

### Step 4.4: Extract Call Features Functions

**Goal:** Move call feature functions to `features.c` / `features.h`.

**Changes:**
- Create `features.h`, `features.c` with call forwarding, voicemail, etc.
- Functions: `CFCheck()`, `CFToggle()`, `CFVMailSet()`, `FollowMe()`, etc.
- Update Makefile
- Test: Test all call features

**Test:**
```bash
# Test:
# - Call forwarding
# - Voicemail forwarding
# - Follow-me
# - Ring delay
```

**Files:** `features.c`, `features.h`, `pbx3cagi.c`, `Makefile`

---

### Step 4.5: Extract Utility Functions

**Goal:** Move utility functions to `utils.c` / `utils.h`.

**Changes:**
- Create `utils.h`, `utils.c` with `GetExt()`, `Mangle()`, `StripPreselect()`, etc.
- Update Makefile
- Test: Verify utilities work

**Test:**
```bash
# Test number manipulation
# Test string transformations
```

**Files:** `utils.c`, `utils.h`, `pbx3cagi.c`, `Makefile`

---

### Step 4.6: Reduce Global Variables

**Goal:** Create context structure to pass instead of globals.

**Changes:**
- Create `CallContext` struct with common fields
- Pass context to functions instead of using globals
- Start with one function, expand gradually
- Test: Verify behavior unchanged

**Test:**
```bash
# Test affected functions
# Verify no regressions
```

**Files:** `pbx3cagi.c`, `pbx3cagi.h`

---

## Phase 5: Code Quality

### Step 5.1: Remove Dead Code

**Goal:** Remove commented-out code blocks.

**Changes:**
- Remove commented code (lines 3304-3306, etc.)
- Remove unused function declarations
- Test: Compile, verify no issues

**Test:**
```bash
# Compile
# Verify no warnings about unused code
```

**Files:** `pbx3cagi.c`, `pbx3cagi.h`

---

### Step 5.2: Fix TODO Comments

**Goal:** Address or document TODO items.

**Changes:**
- Line 3335: Fix or document RDNIS handling issue
- Other TODOs: Address or create tickets
- Test: Verify fixes work

**Test:**
```bash
# Test RDNIS scenarios
# Verify behavior correct
```

**Files:** `pbx3cagi.c`

---

### Step 5.3: Standardize Indentation

**Goal:** Consistent indentation throughout.

**Changes:**
- Choose tabs or spaces (recommend spaces)
- Run formatter or fix manually
- Test: Compile, verify no functional changes

**Test:**
```bash
# Compile
# Visual review
```

**Files:** `pbx3cagi.c`, `pbx3cagi.h`

---

### Step 5.4: Fix Typos and Comments

**Goal:** Clean up comments and fix typos.

**Changes:**
- Line 38: "mesage" → "message"
- Fix other typos
- Improve unclear comments
- Test: Compile, verify

**Test:**
```bash
# Compile
# Review comments
```

**Files:** `pbx3cagi.c`, `pbx3cagi.h`

---

### Step 5.5: Add Function Documentation

**Goal:** Document function contracts.

**Changes:**
- Add comments for function parameters
- Document return values
- Document side effects
- Test: Verify documentation accurate

**Test:**
```bash
# Review documentation
# Verify matches implementation
```

**Files:** `pbx3cagi.c`, `pbx3cagi.h`

---

## Testing Strategy

### After Each Step

1. **Compile:** `make clean && make`
2. **Basic Test:** Make test call, verify routing works
3. **Feature Test:** Test affected feature specifically
4. **Log Review:** Check for errors/warnings
5. **Regression Test:** Verify unrelated features still work

### Full System Test (After Each Phase)

- Inbound calls (various routes)
- Outbound calls (trunks, routes)
- IVR menus
- Queues (ACD, ring groups)
- Call forwarding (all types)
- Voicemail
- Agent operations
- Call recording

---

## Risk Mitigation

1. **Git Commits:** Commit after each step (easy rollback)
2. **Staging:** Test on staging system first
3. **Gradual Rollout:** Deploy to subset of systems initially
4. **Monitoring:** Watch logs closely after each change
5. **Rollback Plan:** Keep previous version ready

---

## Estimated Timeline

- **Phase 1 (Security):** 2-3 weeks (critical)
- **Phase 2 (String Safety):** 1-2 weeks
- **Phase 3 (Error Handling):** 1 week
- **Phase 4 (Organization):** 2-3 weeks
- **Phase 5 (Quality):** 1 week

**Total:** 7-10 weeks for complete plan

---

## Priority Order

1. **Step 1.1-1.6** (SQL Injection) - CRITICAL
2. **Step 2.1-2.6** (String Safety) - HIGH
3. **Step 3.1-3.3** (Error Handling) - MEDIUM
4. **Step 4.1-4.6** (Organization) - MEDIUM
5. **Step 5.1-5.5** (Quality) - LOW

---

## Notes

- Start with Phase 1 (security) - most critical
- Each step should be small enough to test independently
- Don't skip testing steps
- Document any issues encountered
- Update this plan as you learn

---

*This plan is a living document. Update as you progress through steps.*
