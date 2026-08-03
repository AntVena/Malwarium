#!/usr/bin/env bash
# run.sh — rebuild the engine and (re)launch the native host preview window.
#
# Each run regenerates assets, does a fast incremental build of malwarium_host,
# kills any running instance, and relaunches it detached so the window stays up
# after this shell exits. Safe to run repeatedly while iterating.
#
#   bash tools/run.sh        (or the /play slash command in Claude Code)
set -euo pipefail
cd "$(dirname "$0")/.."

python3 tools/gen_assets.py
cmake -S . -B build >/dev/null
cmake --build build --target malwarium_host dump_frame

# Refresh the preview-panel snapshot gallery (tools/preview_page/*.png) so the
# in-chat panel tracks the latest build alongside the native window.
for spec in "rest:" "cursor:carousel" "stat:stat crit" "textonly:carousel textonly"; do
  name="${spec%%:*}"; flags="${spec#*:}"
  ./build/dump_frame 0 "tools/preview_page/$name.ppm" $flags >/dev/null
  sips -s format png "tools/preview_page/$name.ppm" --out "tools/preview_page/$name.png" >/dev/null 2>&1
  rm -f "tools/preview_page/$name.ppm"
done

pkill -x malwarium_host 2>/dev/null || true   # close the previous window first
sleep 0.2
nohup ./build/malwarium_host >/tmp/malwarium_host.log 2>&1 &
disown || true
echo "launched malwarium_host (pid $!) — Z=A · X=B · C=C · U=UI mode · H=hungry · Esc=quit"
