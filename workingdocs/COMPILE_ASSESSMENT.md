# PBX3AGI Compile Assessment

**Date:** 2026-02-09  
**Conclusion:** Builds on **macOS** (using `bsd_compat.h` → system `<string.h>`) and on **Linux** with **libbsd-dev** installed. Same source compiles on both; use macOS for quick test compiles, then pull to a Linux builder—ensure the builder has `libbsd-dev` (e.g. `apt-get install libbsd-dev`) so it compiles without extra steps.

---

## 1. Actual Build Result (macOS / clang)

```text
gcc -c -W -lbsd -std=c99 pbx3cagi.c  
clang: warning: -lbsd: 'linker' input unused [-Wunused-command-line-argument]
pbx3cagi.c:26:10: fatal error: 'bsd/string.h' file not found
   26 | #include <bsd/string.h>
```

**Outcome:** Build fails on this machine (macOS, clang) because `<bsd/string.h>` is not present.

---

## 2. Dependencies

### 2.1 libbsd (required)

- **Use:** `strlcpy`, `strlcat` from `<bsd/string.h>` in:
  - `pbx3cagi.c`
  - `cagi.c`
- **Linux (Debian/Ubuntu):** Install development package:
  ```bash
  sudo apt-get install libbsd-dev
  ```
- **macOS:** System `<string.h>` already provides `strlcpy`/`strlcat`, but the code includes `<bsd/string.h>`. So either:
  - Install libbsd (e.g. `brew install libbsd`) and ensure the compiler finds it, or
  - Add a small compat layer so that on macOS you use `<string.h>` and on Linux `<bsd/string.h>` (see below).

### 2.2 Other system headers

- Standard C and POSIX: `<signal.h>`, `<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<time.h>`, `<sys/types.h>`, `<unistd.h>`, `<sys/wait.h>` — all standard.
- **SQLite:** Uses bundled `sqlite3.c` / `sqlite3.h` — no extra install needed.

---

## 3. Makefile

- **CFLAGS:** `-lbsd` is a **linker** flag. It should not be in `CFLAGS` (used for `-c` compiles).  
  - **Effect:** Compiler warns “linker input unused” (as seen).  
  - **Fix:** Remove `-lbsd` from `CFLAGS` and keep it only on the link line (it’s already there).
- **Link line** already has `-pthread -ldl -lbsd` — correct for the final binary.

---

## 4. Header / Implementation Mismatches (non-blocking)

- **GetState:**  
  - Header: `char* GetState();`  
  - Implementation: `char *GetState(char *cluster);`  
- **CheckTime:**  
  - Header: `char* CheckTime();`  
  - Implementation: `char *CheckTime(char *cluster);`  

In C, an empty `()` in the header means “unspecified parameters”, so the project still compiles (and on Linux you may get warnings). Call sites in this codebase pass `cluster` correctly. Fixing the prototypes to `(char *cluster)` in the header would remove warnings and match the implementation.

---

## 5. Will It Compile?

| Environment | Will it compile? | Notes |
|-------------|-------------------|--------|
| **Linux (Debian/Ubuntu)** | **Yes**, after installing libbsd and fixing CFLAGS | `apt-get install libbsd-dev`, remove `-lbsd` from CFLAGS. |
| **macOS (this machine)** | **No**, as-is | `<bsd/string.h>` not found. Install libbsd or add compat. |
| **Linux without libbsd-dev** | **No** | Same missing header. |

So: **it will compile on a Linux system that has libbsd-dev and a corrected Makefile.** It will not compile on a default macOS setup without one of the changes below.

---

## 6. Recommended Fixes (minimal, for “will it compile?”)

### 6.1 Makefile: stop passing `-lbsd` when compiling

In `csource/Makefile`, change the first line so `-lbsd` is not in CFLAGS:

```makefile
# Before
CFLAGS=-c -W -lbsd -std=c99

# After (keep -lbsd only on link line)
CFLAGS=-c -W -std=c99
```

The link rule already has `-lbsd`; leave it there.

### 6.2 Make `<bsd/string.h>` work on Linux and macOS

**Option A – Linux only (e.g. your target PBX):**  
Install libbsd and use it:

```bash
sudo apt-get install libbsd-dev
```

No code change. After fixing CFLAGS as above, the project should compile.

**Option B – Portable (Linux + macOS):**  
Use a small compat header so macOS uses its built-in `strlcpy`/`strlcat` and Linux uses libbsd.

1. Add a compat header, e.g. `csource/bsd_compat.h`:

```c
#ifndef BSD_COMPAT_H
#define BSD_COMPAT_H
#if defined(__APPLE__) || defined(__FreeBSD__)
#include <string.h>
/* macOS/BSD have strlcpy/strlcat in string.h */
#else
#include <bsd/string.h>
#endif
#endif
```

2. In `pbx3cagi.c` and `cagi.c`, replace:

   - `#include <bsd/string.h>`  
   with  
   - `#include "bsd_compat.h"`

3. On Linux, keep `libbsd-dev` installed so `<bsd/string.h>` is available when not on Apple/BSD.
4. Makefile: remove `-lbsd` from CFLAGS (as in 6.1).

Then:

- **Linux (with libbsd-dev):** compiles.
- **macOS:** compiles using system `string.h` and your compat header.

---

## 7. Summary

- **Will it compile as-is on a random system?** No: dependency on `<bsd/string.h>` and CFLAGS misuse.
- **Will it compile on a Linux PBX with libbsd-dev and a one-line Makefile fix?** Yes.
- **Will it compile on macOS?** Only after adding the compat header (and fixing CFLAGS), or installing libbsd and pointing the compiler at it.

Recommended next steps: apply the CFLAGS fix, then either (A) document “build on Debian/Ubuntu with libbsd-dev” or (B) add `bsd_compat.h` and the two include changes for portability.
