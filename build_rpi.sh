#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-rpi"
JOBS=$(nproc 2>/dev/null || echo 2)

# Detect if we're running on the Pi itself or cross-compiling
if [[ "$(uname -m)" == arm* ]] || [[ "$(uname -m)" == aarch64 ]]; then
    echo "── Building natively on Raspberry Pi ──"
    cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DPLATFORM_RPI=ON
else
    echo "── Cross-compiling for Raspberry Pi ──"
    if ! command -v arm-linux-gnueabihf-gcc &>/dev/null; then
        echo "ERROR: Cross-compiler not found."
        echo "Install with: sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf"
        exit 1
    fi
    cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DPLATFORM_RPI=ON \
        -DCMAKE_TOOLCHAIN_FILE="$PROJECT_ROOT/rpi-toolchain.cmake"
fi

cmake --build "$BUILD_DIR" -j"$JOBS"

echo "──────────────────────────────────────────"
echo "RPi build complete: ${BUILD_DIR}/dungeons"
echo ""
echo "To deploy to your Pi:"
echo "  scp -r ${BUILD_DIR}/dungeons ${BUILD_DIR}/assets pi@<PI_IP>:~/dungeons/"
echo "  ssh pi@<PI_IP> 'cd ~/dungeons && ./dungeons'"
echo "──────────────────────────────────────────"
