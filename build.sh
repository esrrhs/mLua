#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

echo "==> Configuring mLua (Release)..."
cmake -B "${BUILD_DIR}" -S "${SCRIPT_DIR}" -DCMAKE_BUILD_TYPE=Release

echo "==> Building mLua..."
cmake --build "${BUILD_DIR}" --config Release -j"$(nproc 2>/dev/null || echo 2)"

# Ensure libmluacore.so is available at the project root for Lua scripts
if [ -f "${BUILD_DIR}/bin/libmluacore.so" ]; then
    cp -f "${BUILD_DIR}/bin/libmluacore.so" "${SCRIPT_DIR}/libmluacore.so"
elif [ -f "${BUILD_DIR}/bin/libmluacore.dylib" ]; then
    cp -f "${BUILD_DIR}/bin/libmluacore.dylib" "${SCRIPT_DIR}/libmluacore.dylib"
elif [ -f "${BUILD_DIR}/bin/mluacore.dll" ]; then
    cp -f "${BUILD_DIR}/bin/mluacore.dll" "${SCRIPT_DIR}/mluacore.dll"
fi

echo "==> Running tests..."
ctest --test-dir "${BUILD_DIR}" --output-on-failure

echo "==> Build successful"
echo "    library: ${SCRIPT_DIR}/libmluacore.so"
echo "    test:    ${BUILD_DIR}/bin/test_bin"
if [ -x "${BUILD_DIR}/bin/plua" ]; then
    echo "    plua:    ${BUILD_DIR}/bin/plua"
fi
if [ -x "${BUILD_DIR}/bin/proto" ]; then
    echo "    proto:   ${BUILD_DIR}/bin/proto"
fi
