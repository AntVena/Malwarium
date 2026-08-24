#!/usr/bin/env bash
# gates.sh — the build/test cycle: the native suite, then the S3 firmware build.
#
# QUIET ON SUCCESS, LOUD ON FAILURE. Everything goes to a log; only failures are
# printed, and only the lines that matter. A green run is three lines, so running
# the gates is never a reason to skip running the gates.
#
# The two tiers answer different questions and neither substitutes for the other.
# The native suite is the fast one and covers `src/core`. The S3 build is the only
# thing that compiles `src/platform/esp32`, so a green native run says nothing
# about whether the firmware still links — that is why it is here and not optional.
#
#   ./tools/gates.sh              native + S3 (the normal cycle)
#   ./tools/gates.sh --native     native only, for a tight edit loop
#   ./tools/gates.sh --verbose    stream everything instead of logging it
#
# Exits non-zero if any tier fails. CI runs the same two tiers as separate jobs
# (.github/workflows/gates.yml) so a failure names which one broke. CI also runs
# `make pedia-check`, which is not here on purpose: it guards a publish, not a
# build, and paying for it on every edit loop would buy nothing the loop can act on.
set -uo pipefail

cd "$(dirname "$0")/.." || exit 1

NATIVE_ONLY=0
VERBOSE=0
for arg in "$@"; do
    case "$arg" in
        --native)  NATIVE_ONLY=1 ;;
        --verbose) VERBOSE=1 ;;
        -h|--help) sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "gates.sh: unknown option '$arg'" >&2; exit 2 ;;
    esac
done

LOG="${TMPDIR:-/tmp}/malwarium-gates.log"
: > "$LOG"

# Run a step, hiding its output unless it fails (or --verbose). The log is
# APPENDED across steps so a failure report can show what led into it.
run() {
    local label="$1"; shift
    if [ "$VERBOSE" = 1 ]; then
        "$@" 2>&1 | tee -a "$LOG"
        return "${PIPESTATUS[0]}"
    fi
    if ! "$@" >> "$LOG" 2>&1; then
        echo "FAIL: $label" >&2
        echo "--- from $LOG ---" >&2
        # The interesting lines, not the whole build: compiler errors, ctest's
        # failure list and PlatformIO's summary all match one of these. Fall back
        # to the tail when nothing does — a step can fail in a way no pattern
        # anticipated (a Python traceback, a missing tool), and "it failed" with
        # no output is the one report that helps nobody.
        local hits
        hits=$(grep -nE "error:|FAILED|\*\*\*|Error [0-9]|tests? failed" "$LOG" | tail -25)
        if [ -n "$hits" ]; then
            printf '%s\n' "$hits" >&2
        else
            tail -25 "$LOG" >&2
        fi
        return 1
    fi
    return 0
}

# The pantry's ITEMS glyphs are generated from tools/gen_item_icons.py, and the PNGs
# it owns are committed — so this catches a recipe edited without regenerating, and an
# icon hand-edited out from under its recipe. Runs before codegen, because a stale
# glyph would otherwise be compiled into the atlas without complaint.
run "item icons" python3 tools/gen_item_icons.py --check || exit 1

# The combat screen's strike marks and fight-status glyphs, on the same footing and for
# the same reasons (tools/gen_fight_art.py). This one also enforces the margin a strike
# cell keeps, so a recipe that grew past its frame fails here rather than shipping a
# slash with its point quietly cut off.
run "fight art" python3 tools/gen_fight_art.py --check || exit 1

# src/generated is derived from assets/ and not committed, so a clean checkout has
# no engine to compile. Cheap enough to always run.
run "codegen" python3 tools/gen_assets.py || exit 1

run "cmake configure" cmake -S . -B build || exit 1
run "native build"    cmake --build build || exit 1
run "native tests"    ctest --test-dir build --output-on-failure || exit 1
echo "native  ok"

if [ "$NATIVE_ONLY" = 1 ]; then
    echo "(S3 skipped: --native)"
    exit 0
fi

run "S3 firmware build" pio run -e waveshare_s3_154 || exit 1
echo "S3      ok"
