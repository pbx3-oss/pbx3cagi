# PBX3AGI Code Assessment

**Date:** 2026-02-09  
**Codebase:** pbx3cagi-1.0.0  
**Language:** C  
**Lines of Code:** ~3,400 (pbx3cagi.c)

---

## Executive Summary

**pbx3cagi** is a C-based Asterisk AGI (Asterisk Gateway Interface) application that handles call routing, IVR menus, queues, call forwarding, voicemail, and other PBX features for PBX3. The code is **functional but needs security hardening**—it has SQL injection vulnerabilities and maintainability challenges due to its monolithic structure and heavy use of global state.

**Overall Verdict:** Works for its purpose but has **critical security issues** (SQL injection) and **maintainability challenges** (monolithic structure, global state). The codebase shows evolution over time with accumulated technical debt.

---

## Code Structure

- **Main file:** `pbx3cagi.c` (~3,397 lines)
- **Header:** `pbx3cagi.h` (122 lines)
- **Dependencies:** `cagi.c` (AGI library wrapper), `sqlite3.c` (embedded SQLite)
- **Build:** Simple Makefile, compiles to `pbx3cagi` binary
- **Architecture:** Procedural, function-based design

---

## Strengths

### 1. **Functionality**
- Handles comprehensive PBX features: call routing, IVR, queues, call forwarding, voicemail, agent management, recording
- Multi-tenant/cluster support built-in
- SQLite integration with retry logic for database locks

### 2. **String Safety (Partial)**
- Uses `strlcpy`, `strlcat`, `snprintf` in many places
- Uses BSD string functions (`<bsd/string.h>`)

### 3. **Debugging Support**
- Debug mode with function tracing
- Verbose logging via AGI

### 4. **Memory Model**
- Stack-based buffers; no dynamic allocation visible
- Reduces risk of memory leaks/use-after-free

---

## Critical Issues

### 1. **SQL Injection Vulnerabilities** 🔴 CRITICAL

**Problem:** Direct string interpolation in SQL queries without parameterization.

**Examples:**
```c
snprintf(myQuery, sizeof(myQuery), 
    "SELECT callerid from ipphone WHERE pkey='%s' AND cluster='%s'", 
    callerid, myCluster);
sqlQuery(myQuery);
```

**Risk:** User-controlled inputs (`callerid`, `extension`, `PARM_KEY`, etc.) flow directly into SQL queries. Despite using `sqlite3_prepare_v2`, the queries are built via string concatenation, making them vulnerable.

**Impact:** Database compromise, data exfiltration, privilege escalation.

**Fix Required:** Use parameterized queries with `sqlite3_bind_*` functions.

---

### 2. **String Safety Inconsistencies** 🟡 HIGH

**Problem:** Mix of safe and unsafe string functions.

**Unsafe functions still used:**
- `strcpy()` - lines 151, 163, 3073, 3172, etc.
- `strcat()` - lines 1571, 1607, 1687, 1750, etc.
- `sprintf()` - lines 99, 110

**Risk:** Buffer overflows if inputs exceed buffer sizes.

**Fix Required:** Replace all unsafe functions with safe alternatives (`strlcpy`, `strlcat`, `snprintf`).

---

### 3. **Code Organization** 🟡 MEDIUM

**Problems:**
- Single 3,400-line file (monolithic)
- Heavy global state (30+ global variables)
- Hard to test and maintain
- Large switch statement in `main()` (cases 1-68+)

**Impact:** Difficult to understand, test, and modify safely.

---

### 4. **Error Handling** 🟡 MEDIUM

**Problems:**
- SQLite errors return `"-1"` or empty strings; inconsistent handling
- Some functions don't check return values
- File operations (`fopen`, `fwrite`) may fail silently

**Example:**
```c
retval = sqlite3_open(SQLITEDB, &handle);
if (retval) {
    // Returns "-1" string, caller may not handle properly
    return "-1";
}
```

---

### 5. **Code Quality Issues** 🟢 LOW

- Commented-out code (e.g., lines 3304-3306)
- TODO comments (line 3335: "ToDo - this RDNIS code is wrong")
- Magic numbers (e.g., `cfTab[50]` with sparse entries)
- Inconsistent indentation (tabs vs spaces)
- Typo: "mesage" instead of "message" (line 38)

---

### 6. **Hardcoded Assumptions** 🟢 LOW

- Database schema assumptions (`master_xref`, `IPphone`, `cluster`, etc.)
- Hardcoded context name `"qrxvtmny"` (line 52)
- Assumes specific table/column names

---

### 7. **Performance** 🟢 LOW

- Opens/closes SQLite connection per query (no connection pooling)
- Retry loop (3 attempts) adds latency on locks
- String operations in loops could be optimized

---

## Architecture Observations

- **Design Pattern:** Procedural, function-based
- **AGI Protocol:** Wrapper (`cagi.h`) abstracts Asterisk communication
- **Database Layer:** Abstraction (`DBQuery`, `sqlQuery`) but not parameterized
- **Feature Organization:** Feature-based functions (`OutTrunk`, `InCall`, `IVR`, etc.)

---

## Recommendations

### Critical (Do First)
1. **Fix SQL Injection:** Parameterize all SQL queries using `sqlite3_bind_*`
2. **Replace Unsafe String Functions:** All `strcpy`/`strcat`/`sprintf` → safe versions

### High Priority
3. **Error Handling:** Consistent error return codes and checking
4. **Input Validation:** Validate all user inputs before use

### Medium Priority
5. **Code Organization:** Split into modules (routing, IVR, voicemail, etc.)
6. **Reduce Global State:** Pass context structs instead of globals
7. **Testing:** Add unit tests for critical functions

### Low Priority
8. **Cleanup:** Remove dead/commented code
9. **Documentation:** Document function contracts and behavior
10. **Performance:** Add connection pooling for SQLite

---

## Rewrite Analysis

See `REWRITE_ANALYSIS.md` for detailed pros/cons and language recommendations.

**TL;DR:** If rewriting, **Go** is recommended. If not rewriting, prioritize SQL injection fixes and string safety improvements.

---

## Files Modified (2026-02-09)

- `pbx3cagi.h`: Fixed SQLite DB path (`/opt/gcs/` → `/opt/pbx3/`)
- `pbx3cagi.c`: Fixed legacy `/opt/sark/` paths → `/opt/pbx3/`
- `compile`: Updated to use `pbx3cagi.c` instead of `sarkhpe.c`

---

*This assessment is for AI agent use in future sessions. Update as code evolves.*
