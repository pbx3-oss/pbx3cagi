#!/usr/bin/env bash
# Stage pbx3cagi binaries for debuild (Architecture: all; arm64 required on AWS fleet).
#
# Usage (from repo root):
#   ./scripts/seed-deb-binaries.sh --build-local          # make arm64 on golden
#   PBX3CAGI_ARM64=/path/pbx3cagi ./scripts/seed-deb-binaries.sh
#   PBX3CAGI_AMD64=/path/pbx3cagi ./scripts/seed-deb-binaries.sh   # optional plug-in
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PKG="$ROOT/pbx3cagi-1.0.0"
CSOURCE="$PKG/csource"
DEST="$PKG/usr/share/asterisk/agi-bin"
BUILD_LOCAL=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-local) BUILD_LOCAL=1; shift ;;
    -h|--help)
      sed -n '2,10p' "$0"
      exit 0
      ;;
    *) echo "unknown arg: $1" >&2; exit 1 ;;
  esac
done

mkdir -p "$DEST"
# Do not rm existing amd64 when only refreshing arm64
if [[ -z "${PBX3CAGI_AMD64:-}" && "$BUILD_LOCAL" -eq 0 ]]; then
  rm -f "$DEST/pbx3cagi.arm64"
fi

stage_one() {
  local label="$1" src="$2" dest="$DEST/pbx3cagi.$label"
  if [[ ! -f "$src" ]]; then
    echo "ERROR: missing $label binary: $src" >&2
    exit 1
  fi
  cp -f "$src" "$dest"
  chmod 755 "$dest"
  echo "staged $dest ($(file -b "$dest"))"
}

if [[ "$BUILD_LOCAL" -eq 1 ]]; then
  echo "building in $CSOURCE ..."
  make -C "$CSOURCE" clean all
  arch="$(uname -m)"
  case "$arch" in
    x86_64|amd64) stage_one amd64 "$CSOURCE/pbx3cagi" ;;
    aarch64|arm64) stage_one arm64 "$CSOURCE/pbx3cagi" ;;
    *) echo "ERROR: --build-local unsupported arch: $arch" >&2; exit 1 ;;
  esac
fi

if [[ -n "${PBX3CAGI_AMD64:-}" ]]; then
  stage_one amd64 "$PBX3CAGI_AMD64"
fi

if [[ -n "${PBX3CAGI_ARM64:-}" ]]; then
  stage_one arm64 "$PBX3CAGI_ARM64"
fi

if [[ ! -f "$DEST/pbx3cagi.arm64" ]]; then
  echo "ERROR: $DEST/pbx3cagi.arm64 not staged (required for AWS arm64 fleet)" >&2
  echo "  ./scripts/seed-deb-binaries.sh --build-local   # on golden" >&2
  echo "  PBX3CAGI_ARM64=./csource/pbx3cagi ./scripts/seed-deb-binaries.sh" >&2
  exit 1
fi

if [[ -f "$DEST/pbx3cagi.amd64" ]]; then
  echo "OK: arm64 + amd64 staged under $DEST"
else
  echo "OK: arm64 staged under $DEST (amd64 omitted — add later with PBX3CAGI_AMD64=...)"
fi
