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
**Do not** install or scp a Mac-built `pbx3cagi` onto golden/lab instances (Mach-O, wrong path). AGI on fleet runs **Linux aarch64** at `/usr/share/asterisk/agi-bin/pbx3cagi` → `pbx3cagi.arm64`. Build for package/deploy on the target (or a Linux aarch64 builder with `libbsd-dev`) — **golden has the tools**.

**Linux (Debian/Ubuntu / golden):** Install libbsd dev, then build:
```bash
sudo apt-get install libbsd-dev
cd pbx3cagi-1.0.0/csource && make clean && make
# install (example):
#   sudo cp -a pbx3cagi /usr/share/asterisk/agi-bin/pbx3cagi.arm64
```
*Linux builder images and CI should have `libbsd-dev` (or distro equivalent) so that a tree that compiles on macOS also compiles on Linux.*
