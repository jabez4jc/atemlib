#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK_BASE="/Users/jnt/Downloads/Blackmagic_ATEM_Switchers_SDK_10.2.1/Blackmagic ATEM Switchers SDK 10.2.1/Mac OS X"
SDK_INCLUDE_DIR="${SDK_BASE}/include"
OUT_DIR="${REPO_ROOT}/native/atem_bridge/build"
OUT_LIB="${OUT_DIR}/libatem_bridge.dylib"

if [[ ! -f "${SDK_INCLUDE_DIR}/BMDSwitcherAPI.h" ]]; then
  echo "ATEM SDK header not found at: ${SDK_INCLUDE_DIR}/BMDSwitcherAPI.h" >&2
  exit 1
fi

mkdir -p "${OUT_DIR}"

clang++ -std=c++17 -dynamiclib -fPIC \
  -I"${SDK_INCLUDE_DIR}" \
  "${REPO_ROOT}/native/atem_bridge/atem_bridge.cpp" \
  -framework CoreFoundation \
  -o "${OUT_LIB}"

echo "Built bridge: ${OUT_LIB}"
echo "Export for runtime: DYLD_LIBRARY_PATH=${OUT_DIR}:\$DYLD_LIBRARY_PATH"
