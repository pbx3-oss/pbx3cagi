#!/usr/bin/env bash
# Clean, seed AGI binaries, and build pbx3cagi _all.deb.
#
# Usage (from repo root, on Linux with debuild):
#   PBX3CAGI_AMD64=... PBX3CAGI_ARM64=... ./scripts/build-deb.sh
#   ./scripts/build-deb.sh --build-local   # arm64 on golden; amd64 optional later
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PKG="$ROOT/pbx3cagi-1.0.0"
SEED_ARGS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-local) SEED_ARGS+=(--build-local); shift ;;
    -h|--help)
      sed -n '2,8p' "$0"
      exit 0
      ;;
    *) echo "unknown arg: $1" >&2; exit 1 ;;
  esac
done

command -v debuild >/dev/null || { echo "ERROR: debuild not found" >&2; exit 1; }

"$ROOT/scripts/seed-deb-binaries.sh" "${SEED_ARGS[@]}"

cd "$PKG"
rm -rf debian/pbx3cagi debian/.debhelper
debuild -us -uc -b

echo ""
echo "Built:"
ls -1 "$PKG/../"pbx3cagi_*.deb 2>/dev/null || ls -1 ../*.deb 2>/dev/null
