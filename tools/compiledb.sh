#!/usr/bin/env bash
# compiledb.sh — regenerate the two compilation databases clangd reads.
#
# The project builds on two toolchains and clangd cannot infer either one: the host
# tier's flags come from CMake, the device tier's from PlatformIO, and the device tier
# is the one that breaks visibly without them (Arduino.h "file not found", every
# Arduino/ESP-IDF symbol undeclared, and the real diagnostics buried underneath).
#
# Run this after a fresh clone, and again whenever the include set moves — a new lib_deps
# entry, an Arduino core bump, a new build flag. Both outputs are git-ignored build
# artefacts; the .clangd files that point at them are what is committed.
set -euo pipefail
cd "$(dirname "$0")/.."

FW_ENV=waveshare_s3_154

echo "host   -> build/compile_commands.json"
cmake -S . -B build >/dev/null

# PlatformIO's compiledb target writes to the project root and offers no way to redirect
# it, so the file is moved into place. Root is where clangd would find it FIRST, ahead of
# the .clangd files below it — leaving it there would check the whole repo, host code
# included, against the Xtensa toolchain.
echo "device -> build-device/compile_commands.json"
mkdir -p build-device
pio run -t compiledb -e "$FW_ENV" >/dev/null
mv compile_commands.json build-device/compile_commands.json

echo "ok — clangd reads build/ for everything but src/platform/esp32/, which reads build-device/"
