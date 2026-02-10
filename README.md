# pbx3agi

PBX3 AGI scripts and utilities.

## Build

**macOS:** No extra deps. From the source tree:
```bash
cd pbx3cagi-1.0.0/csource && make clean && make
```

**Linux (Debian/Ubuntu):** Install libbsd dev, then build:
```bash
sudo apt-get install libbsd-dev
cd pbx3cagi-1.0.0/csource && make clean && make
```
*Linux builder images and CI should have `libbsd-dev` (or distro equivalent) so that a tree that compiles on macOS also compiles on Linux.*
