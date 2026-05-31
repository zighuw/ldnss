#!/bin/bash
# ============================================================================
# ldnss — Package Script
#
# Collects all runtime dependencies from a completed build/ into output/,
# compresses binaries with UPX, then optionally cleans build intermediates.
#
# Prerequisite: run ./build.sh first (build/ must exist with compiled EXEs).
#
# Usage:
#   ./package.sh              Package + clean build/
#   ./package.sh --no-clean   Keep build/ directory after packaging
# ============================================================================
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
OUTPUT_DIR="$PROJECT_DIR/output"

KEEP_BUILD=0
if [[ "${1:-}" == "--no-clean" ]]; then
    KEEP_BUILD=1
fi

# Guard: build/ must exist
if [[ ! -f "$BUILD_DIR/src/ldnss.exe" || ! -f "$BUILD_DIR/src/ldnss-cli.exe" ]]; then
    echo "ERROR: build/ not found or incomplete — run ./build.sh first"
    exit 1
fi

echo "============================================"
echo " ldnss — Package"
echo "============================================"

# ------------------------------------------------------------------
# 1. Prepare output directory
# ------------------------------------------------------------------
echo ""
echo "[1/6] Preparing output directory..."
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

# ------------------------------------------------------------------
# 2. Copy executables
# ------------------------------------------------------------------
echo ""
echo "[2/6] Copying executables..."
cp "$BUILD_DIR/src/ldnss.exe" "$OUTPUT_DIR/"
cp "$BUILD_DIR/src/ldnss-cli.exe" "$OUTPUT_DIR/"

# ------------------------------------------------------------------
# 3. Deploy Qt runtime (GUI exe pulls full Qt; CLI only needs Qt6Core)
# ------------------------------------------------------------------
echo ""
echo "[3/6] Running windeployqt..."
cd "$OUTPUT_DIR"
windeployqt ldnss.exe --no-translations --no-compiler-runtime --no-system-d3d-compiler

# Remove plugins we don't use: Qt6Network, generic, networkinformation, tls, qgif
rm -f Qt6Network.dll
rm -rf generic/ networkinformation/ tls/
rm -f imageformats/qgif.dll

# ------------------------------------------------------------------
# 4. Copy non-Qt runtime DLLs
#     These are the MSYS2/ucrt64 runtime dependencies not handled by
#     windeployqt.  The list comes from: ldd output/*.exe | grep ucrt64
#     Static linking eliminated libsndfile + 7 codecs + libportaudio.
# ------------------------------------------------------------------
echo ""
echo "[4/6] Copying non-Qt runtime DLLs..."

NONQT_DLLS=(
    # Compiler runtimes
    libgcc_s_seh-1.dll
    libwinpthread-1.dll
    libstdc++-6.dll
    # libebur128 (no static .a in MSYS2)
    libebur128.dll
    # Qt transitive deps
    libb2-1.dll
    libdouble-conversion.dll
    libicuin78.dll
    libicuuc78.dll
    libicudt78.dll
    libpcre2-16-0.dll
    zlib1.dll
    libzstd.dll
    libfreetype-6.dll
    libharfbuzz-0.dll
    libmd4c.dll
    libpng16-16.dll
    libbrotlidec.dll
    libbz2-1.dll
    libglib-2.0-0.dll
    libgraphite2.dll
    libbrotlicommon.dll
    libpcre2-8-0.dll
    libintl-8.dll
    libiconv-2.dll
)

COPIED=0
for dll in "${NONQT_DLLS[@]}"; do
    cp "/ucrt64/bin/$dll" "$OUTPUT_DIR/"
    COPIED=$((COPIED + 1))
done

echo "   Copied $COPIED non-Qt DLL(s)"

# ------------------------------------------------------------------
# 5. UPX compress
#     Compress all EXEs and top-level DLLs with UPX --best (NRV only).
#     Plugin DLLs in platforms/ imageformats/ styles/ are SKIPPED —
#     UPX corrupts the PE structure that Qt's plugin loader depends on.
# ------------------------------------------------------------------
echo ""
echo "[5/6] Compressing with UPX --best (NRV, no LZMA)..."
# --lzma (LZMA) can produce corrupted EXEs with this UPX version (5.1.1),
# causing STATUS_DLL_INIT_FAILED (0xc0000142). NRV is stable.
# Non-plugin DLLs have been tested end-to-end with UPX NRV — they load and
# run correctly (libebur128, ICU, libstdc++, Qt runtime, etc.).
upx --best "$OUTPUT_DIR"/*.exe
upx --best "$OUTPUT_DIR"/*.dll

# ------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------
echo ""
echo "============================================"
echo " Package complete"
echo "============================================"

EXE_COUNT=$(ls "$OUTPUT_DIR"/*.exe 2>/dev/null | wc -l)
DLL_COUNT=$(ls "$OUTPUT_DIR"/*.dll 2>/dev/null | wc -l)
PLUGIN_COUNT=$(ls "$OUTPUT_DIR"/platforms/*.dll "$OUTPUT_DIR"/imageformats/*.dll "$OUTPUT_DIR"/styles/*.dll 2>/dev/null | wc -l)
TOTAL_FILES=$((EXE_COUNT + DLL_COUNT + PLUGIN_COUNT))
TOTAL_SIZE=$(du -sh "$OUTPUT_DIR" 2>/dev/null | cut -f1)

echo "  Executables : $EXE_COUNT"
echo "  Runtime DLLs: $DLL_COUNT"
echo "  Plugin DLLs : $PLUGIN_COUNT"
echo "  Total files : $TOTAL_FILES"
echo "  Total size  : $TOTAL_SIZE"
echo "  Output dir  : $OUTPUT_DIR"

# ------------------------------------------------------------------
# 6. Clean build intermediates
# ------------------------------------------------------------------
if [[ $KEEP_BUILD -eq 0 ]]; then
    echo ""
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    echo "  Removed $BUILD_DIR"
else
    echo ""
    echo "--no-clean: keeping $BUILD_DIR"
fi

echo ""
echo "Done."
