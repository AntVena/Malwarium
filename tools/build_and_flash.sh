#!/usr/bin/env bash
#
# build_and_flash.sh — the full "make a change land on the device" pipeline,
# one command. New to the project? Read this file top to bottom: each step
# below is exactly one script this repo already has, run in the order a
# change actually needs them, so you can see where YOUR edit lands without
# hunting through docs. There is no new logic here — this is pure
# orchestration; every real step delegates to the script that already owns
# that concern, so the actual behaviour lives in exactly ONE place, not two.
#
# The pipeline, and why it's in this order:
#
#   1. gen_assets.py — assets/**.png (+ PAL_CORE.json) -> src/generated/.
#      This output is git-ignored and, deliberately, NOT run automatically by
#      `pio run` (see platformio.ini's own comment on this — a python step on
#      every device build would be a surprise). Skip it and a new or renamed
#      sprite keeps compiling in as whatever src/generated/ last had — often
#      no error, just the OLD art (or the generic fallback) shipping silently.
#      If your change didn't touch assets/, this still costs ~nothing (pure
#      stdlib, no Pillow) — safe to always run.
#
#   2. `make pedia` (tools/gen_pedia_data.py) — the LIVE firmware content
#      tables (src/core/content/*, src/core/model/combat.cpp, include/
#      tunables.h) -> web/data/pedia_data.js, the file the SD-served web
#      'Pedia site actually reads. Independent of step 1 (different source
#      files) but grouped here for the same reason: it's generated-from-
#      source housekeeping that's easy to forget before a build, and `make
#      pedia-check` (CI) will catch a missed regen anyway — better to just
#      never miss it locally.
#
#   3. tools/flash.sh — compiles the firmware for the target PlatformIO env
#      (now picking up step 1's fresh assets) and flashes it over USB.
#      DELEGATED to, not reimplemented here: flash.sh already owns serial-
#      port autodetection, the --wait-wake light-sleep retry loop, and the
#      post-flash partition/save-usage report, and provision.sh (the
#      fresh-hardware bring-up script) delegates to it the same way this
#      script does — one maintained build+upload primitive, not three.
#      flash.sh's own --pedia <DEST> flag (opt-in SD 'Pedia staging — the
#      web/ bundle lives on the microSD card, not the MCU flash, and staging
#      needs the card in a READER on this computer, not the device) works
#      unmodified through this wrapper: see the passthrough note below.
#
# What this does NOT do: run the native test gates (`ctest` / `pio test -e
# native`, or the `gate-runner` subagent) or a device-side serial smoke
# check. Build-and-flash answers "does it compile and reach the board," not
# "is it correct" — run the gates yourself before you trust a change, same
# as always.
#
# Usage:
#   ./tools/build_and_flash.sh                          # everyday reflash, primary board
#   ./tools/build_and_flash.sh --env waveshare_s3_154_bringup  # pin-scan bring-up build
#   ./tools/build_and_flash.sh --wait-wake               # device may be light-sleeping
#   ./tools/build_and_flash.sh --pedia /Volumes/MALWARIUM  # also stage the SD 'Pedia bundle
#   ./tools/build_and_flash.sh --skip-assets             # rare: assets/ untouched, save the pass
#   ./tools/build_and_flash.sh --skip-pedia              # rare: content tables untouched
#
# Any flag this script doesn't recognise (--env, --port, --pedia, --no-upload,
# --wait-wake, ...) is forwarded verbatim, in order, straight to
# tools/flash.sh — so its full flag set works here unmodified. See
# `./tools/flash.sh --help` for the exhaustive list.

set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

usage() { sed -n '2,/^set -euo/p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//; /^set -euo/d'; }

SKIP_ASSETS=0
SKIP_PEDIA=0
FLASH_ARGS=()

while [ $# -gt 0 ]; do
    case "$1" in
        --skip-assets) SKIP_ASSETS=1; shift ;;
        --skip-pedia)  SKIP_PEDIA=1; shift ;;
        -h|--help)     usage; exit 0 ;;
        *) FLASH_ARGS+=("$1"); shift ;;   # unrecognised -> passed straight to flash.sh
    esac
done

# --- 1. sprite/palette codegen (assets/ -> src/generated/) ------------------
if [ "$SKIP_ASSETS" -eq 1 ]; then
    echo ">> [1/3] skipping gen_assets.py (--skip-assets)"
else
    echo ">> [1/3] gen_assets.py — assets/**.png -> src/generated/"
    python3 tools/gen_assets.py
fi

# --- 2. web 'Pedia data regen (content tables -> web/data/pedia_data.js) ----
if [ "$SKIP_PEDIA" -eq 1 ]; then
    echo ">> [2/3] skipping pedia data regen (--skip-pedia)"
else
    echo ">> [2/3] make pedia — content tables -> web/data/pedia_data.js"
    make -C "$repo_root" pedia
fi

# --- 3. build + flash (+ optional SD 'Pedia staging), delegated in full -----
# The `${FLASH_ARGS[@]+"${FLASH_ARGS[@]}"}` expansion (not just "${FLASH_ARGS[@]}")
# is required, not decorative: macOS ships bash 3.2, which throws "unbound
# variable" under `set -u` when a COMPLETELY EMPTY array is expanded with
# "${arr[@]}" (fixed in bash 4.4+, but this repo targets the stock macOS shell).
# The common no-extra-flags invocation hits exactly this — skipping the guard
# means the everyday, argument-less case is the one that always crashes.
echo ">> [3/3] tools/flash.sh ${FLASH_ARGS[*]:-}"
"$repo_root/tools/flash.sh" "${FLASH_ARGS[@]+"${FLASH_ARGS[@]}"}"
