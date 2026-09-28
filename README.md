# pbx3agi

**How this was built:** PBX3 was **designed by humans** in mid/late 2025. Most of the code was **implemented in Cursor** under **human-directed AI** assistance — not unattended generation, and not a claim that AI is a panacea.

**Operator documentation (all PBX3 repos):** [pbx3-docs (MkDocs)](https://pbx3-oss.github.io/pbx3-docs/)

- **Way forward:** classical install / admin procedure pages and the shipped installers in these repositories.
- **Optional co-pilot:** if you already use an AI coding agent, start at [AI-assisted operations](https://pbx3-oss.github.io/pbx3-docs/getting-started/ai-assisted/) — same scripts and human gates. Not unattended; **AI is not a panacea**.

PBX3 AGI scripts and utilities.

## Build

**macOS (host L0 only):** No extra deps. From the source tree:
```bash
cd pbx3cagi-1.0.0/csource && make clean && make
```
**Do not** install or scp a Mac-built `pbx3cagi` onto golden/lab instances (Mach-O, wrong path). AGI on fleet runs **Linux aarch64** at `/usr/share/asterisk/agi-bin/pbx3cagi` → `pbx3cagi.arm64`. Build for package/deploy on a Linux builder with `libbsd-dev` (see Packaging).

**Linux (Debian/Ubuntu):** Install libbsd dev, then build:
```bash
sudo apt-get install libbsd-dev
cd pbx3cagi-1.0.0/csource && make clean && make
# install (example):
#   sudo cp -a pbx3cagi /usr/share/asterisk/agi-bin/pbx3cagi.arm64
```

## Packaging (locked 2026-09-28)

Sailhpe-style **`Architecture: all`** `.deb`: native binaries are compiled **outside** `debuild`, staged, then copied in. No Docker.

| Step | Where | Command / note |
|------|--------|----------------|
| amd64 binary | Lab **`tech@192.168.1.213`** (`~/git/pbx3cagi`, tip of `pbx3-oss/pbx3cagi`) | `cd pbx3cagi-1.0.0/csource && make clean all` → `pbx3cagi` (ELF x86-64) |
| arm64 binary | Lab **`tech@192.168.1.148`** or cloud **golden** (aarch64) | same `make`; need `libbsd-dev` + `build-essential` |
| Assemble deb | Linux with `debuild` (`.148` / golden; **not** ancient Debian 9 on `.213`) | `PBX3CAGI_AMD64=… PBX3CAGI_ARM64=… ./scripts/build-deb.sh` |
| Scripts | repo root | `scripts/seed-deb-binaries.sh` · `scripts/build-deb.sh` |

Bump `pbx3cagi-1.0.0/debian/changelog` **before** `debuild`. Package lands as `pbx3cagi_<ver>_all.deb` (contains `pbx3cagi.arm64` **and** preferably `pbx3cagi.amd64`). Postinst symlinks `pbx3cagi` → `pbx3cagi.$(dpkg --print-architecture)`.

### Release artefacts in git (regress window)

`*.deb` is **gitignored**. Force-add the current release so installs can pull from `main`.

**Keep the last three** release debs tracked (current + two prior) for easy regress. On each new release:

1. `git add -f pbx3cagi_<new>_all.deb`
2. `git rm --cached pbx3cagi_<oldest-of-the-three>_all.deb` (leave file on disk if you want)
3. Commit changelog + deb add/rm together

Do **not** accumulate the full history in git. Older local copies may remain untracked on the Mac.
