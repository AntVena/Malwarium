#!/usr/bin/env bash
# build_web.sh — build the public web demo into pages/play/.
#
# Two stages, because the demo's starting save has to be produced by the engine
# before the engine can be compiled with it:
#
#   1. NATIVE: build gen_demo_save and run it. It seeds a seeded pet through the
#      real Game API, serializes it with the real save writer, checks the blob
#      round-trips, and writes src/generated/demo_save.{cpp,h}.
#   2. WASM: emcmake configures the same tree with MALWARIUM_DEMO set, compiles
#      malcore + src/platform/web + that generated blob, and emits malwarium.js
#      and malwarium.wasm next to the shell page.
#
# The blob is regenerated every run rather than committed, which is what stops it
# going stale: it is always written by the same kSaveVersion that will read it.
#
#   ./tools/build_web.sh                     build into pages/play/
#   ./tools/build_web.sh --serve             build, then serve it on :8000
#
# Needs the Emscripten SDK on PATH (emcmake/emcc). Install:
#   git clone https://github.com/emscripten-core/emsdk && cd emsdk
#   ./emsdk install latest && ./emsdk activate latest && source ./emsdk_env.sh
set -euo pipefail

cd "$(dirname "$0")/.."

SERVE=0
for arg in "$@"; do
    case "$arg" in
        --serve) SERVE=1 ;;
        -h|--help) sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "build_web.sh: unknown option '$arg'" >&2; exit 2 ;;
    esac
done

if ! command -v emcmake >/dev/null 2>&1; then
    echo "build_web.sh: emcmake not on PATH — source your emsdk_env.sh first" >&2
    exit 1
fi

OUT=pages/play

# The atlas: src/generated/assets.cpp is derived from assets/ and not committed, so
# a clean checkout has no engine to compile (same step tools/gates.sh runs).
echo "==> codegen (assets)"
python3 tools/gen_assets.py > /dev/null

echo "==> stage 1: bake the demo save (native)"
cmake -S . -B build > /dev/null
cmake --build build --target gen_demo_save > /dev/null
./build/gen_demo_save --out src/generated

echo "==> stage 2: compile the engine to wasm"
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build build-web --target malwarium_web

mkdir -p "$OUT"
cp build-web/malwarium.js build-web/malwarium.wasm "$OUT/"

echo
echo "built $OUT/"
ls -la "$OUT"/malwarium.js "$OUT"/malwarium.wasm | awk '{printf "  %-28s %8.1f KB\n", $9, $5/1024}'
echo "  (gzip: $(gzip -9 -c "$OUT/malwarium.wasm" | wc -c | awk '{printf "%.1f KB", $1/1024}') wasm over the wire)"

if [ "$SERVE" = 1 ]; then
    echo
    echo "serving pages/ on http://localhost:8000/play/ — Ctrl-C to stop"
    (cd pages && python3 -m http.server 8000)
fi
