#!/usr/bin/env bash
# Records the golden walks again (test/test_native/test_golden_walks.cpp).
#
# Run it when a change MEANS to alter what a walk shows: a new screen, new wording, a
# behaviour change. It rebuilds the native suite, plays every walk, and rewrites
# test/test_native/golden_<walk>.h, so the commit's diff shows which steps moved. Then
# it rebuilds and runs the suite once more, so the new recording is shown to hold.
#
# A refactor that means to change nothing never needs this: a walk that drifts is the
# gate doing its job.
set -euo pipefail
cd "$(dirname "$0")/.."

cmake -S . -B build >/dev/null
cmake --build build --target mal_tests -j"$(nproc)" >/dev/null
MAL_GOLDEN_WRITE="$PWD/test/test_native" ./build/mal_tests >/dev/null
cmake --build build --target mal_tests -j"$(nproc)" >/dev/null
./build/mal_tests >/dev/null
git --no-pager diff --stat -- test/test_native/golden_*.h || true
echo "golden walks recorded"
