#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-web"
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# ─── Verify emsdk ────────────────────────────────────────────────────────────
if ! command -v emcmake &>/dev/null; then
    echo "ERROR: Emscripten not found. Install emsdk and run 'source emsdk_env.sh'" >&2
    exit 1
fi

# ─── Configure & build ───────────────────────────────────────────────────────
emcmake cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j"$JOBS"

echo "──────────────────────────────────────────"
echo "Web build complete: ${BUILD_DIR}"
echo "Files: dungeons.html, dungeons.js, dungeons.wasm, dungeons.data"
echo "──────────────────────────────────────────"

# ─── Open output folder ──────────────────────────────────────────────────────
case "$(uname -s)" in
    Darwin*)  open "$BUILD_DIR" ;;
    Linux*)   xdg-open "$BUILD_DIR" 2>/dev/null || echo "Open: $BUILD_DIR" ;;
    MINGW*|MSYS*|CYGWIN*) explorer.exe "$(cygpath -w "$BUILD_DIR")" ;;
esac

# ─── Optional: serve locally ─────────────────────────────────────────────────
read -rp "Start local server? [y/N] " yn
if [[ "${yn,,}" == "y" ]]; then
    echo "Serving at http://localhost:8080/dungeons.html"
    python3 -m http.server 8080 -d "$BUILD_DIR"
fi
