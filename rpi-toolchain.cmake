# rpi-toolchain.cmake — Cross-compile toolchain for Raspberry Pi Zero 2W
#
# Prerequisites (install on your build machine):
#   sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
#
# Usage:
#   cmake -B build-rpi -DCMAKE_TOOLCHAIN_FILE=rpi-toolchain.cmake -DPLATFORM_RPI=ON
#   cmake --build build-rpi -j

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7l)

# Cross-compiler (armhf for 32-bit Raspberry Pi OS)
set(CMAKE_C_COMPILER   arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# Sysroot — set this to your Pi's root filesystem if cross-compiling with libs
# set(CMAKE_SYSROOT /path/to/rpi-sysroot)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Optimize for Cortex-A53 (Zero 2W's CPU)
set(CMAKE_C_FLAGS_INIT   "-mcpu=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard")
