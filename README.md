# pbx3agi

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
