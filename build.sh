#!/bin/bash
# ============================================================================
# ldnss — Build & Test Script
#
# Performs a clean Release build and runs all tests.  Does NOT package —
# use package.sh separately for deployment to output/.
#
# Usage:
#   ./build.sh
# ============================================================================
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

echo "============================================"
echo " ldnss — Build & Test"
echo "============================================"

# ------------------------------------------------------------------
# 1. Configure (Release, Ninja)
# ------------------------------------------------------------------
echo ""
echo "[1/3] Configuring CMake (Release)..."
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release

# ------------------------------------------------------------------
# 2. Build
# ------------------------------------------------------------------
echo ""
echo "[2/3] Building..."
cmake --build "$BUILD_DIR"

# ------------------------------------------------------------------
# 3. Test
# ------------------------------------------------------------------
echo ""
echo "[3/3] Running tests..."
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo ""
echo "============================================"
echo " Build & test complete"
echo "============================================"
echo "  Build dir: $BUILD_DIR"
echo ""
echo "Run ./package.sh to deploy to output/"
