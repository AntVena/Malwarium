#!/usr/bin/env bash
#
# serve_updates.sh — publish a firmware + web-'Pedia update from this laptop and
# serve it, so a board can update ITSELF over the air. The over-the-air sibling of
# tools/build_and_flash.sh: same "one command, pure orchestration, every real step
# delegated" shape, aimed at the path a real user's device takes rather than the
# USB one only a developer has. Read it top to bottom to see where your edit lands.
#
# WHY TEST THIS WAY. USB flashing is the developer's path; OTA is everyone else's,
# and it exercises code USB never touches — the manifest parser, the SHA-256 gate,
# the OTA slot write, the tar unpack onto SD. A change that flashes perfectly can
# still be unpublishable. Running an update the way a user would, periodically, is
# how that stays true.
#
# The pipeline, and why it's in this order:
#
#   1. gen_assets.py — assets/**.png (+ PAL_CORE.json) -> src/generated/. Same
#      reason build_and_flash.sh runs it first: `pio run` deliberately does NOT,
#      so skipping it silently publishes an image built from stale art.
#
#   2. Optional version bump (--bump). NOT decorative: a device only offers an
#      artifact whose version code is HIGHER than what it is running, so
#      republishing at the same version reports "UP TO DATE" and nothing happens.
#      That is correct behaviour and the single most common way an OTA test looks
#      broken when it isn't. Bump, or expect no offer.
#
#      The three parts, and which one to reach for: `major` is a release. `feature`
#      is a new feature or a small save bump. `fix` is everything else, and it is
#      the one you want nearly every time — it has three digits, so a stretch
#      between two features holds a thousand publishes without borrowing from the
#      number above it.
#
#   3. `make pages BASE=<this laptop>` — builds the firmware image
#      (include/version.h names it), the web bundle tar (web/VERSION names it),
#      and the manifest listing both with their real sizes and digests, into
#      dist/. It validates the result with the DEVICE's own parser before
#      returning, so an unpublishable manifest fails here rather than looking
#      like a dead network from the device's side. It also stages the release
#      pages (pages/) and the boot images the USB flasher writes, so this serves
#      the whole publish rather than only the half a device reads.
#
#      The flasher at /flash/ needs a SECURE CONTEXT — Web Serial is unavailable
#      over plain http to a LAN address, so reach it at http://localhost:<port>
#      from this machine. The device half is unaffected; it only wants bytes.
#
#   4. Optional damage injection (--corrupt / --truncate) — AFTER the manifest has
#      recorded the good size and digest, which is what makes the failure real:
#      the device downloads what it was promised and finds it isn't. These are the
#      failure paths the happy path can't prove.
#
#   5. `python3 -m http.server` over dist/, bound to all interfaces. Its request
#      log is the evidence the device actually fetched the whole image.
#
# POINTING THE DEVICE AT THIS HOST. The compiled-in default is empty, so a stock
# build looks nowhere. Two ways to aim it, printed again at the end with the live
# address filled in:
#
#   * No USB (the honest OTA rehearsal): 'Pedia -> DEVICE SETTINGS -> UPDATE
#     SOURCE, paste the manifest URL. Stored in NVS, which the device-tier
#     resolver prefers over the compiled default (platform/esp32/update_source.h).
#     Confirm it took on CFG -> UPDATES, which prints the host it will call.
#   * With USB (once, to bake in a default):
#     PLATFORMIO_BUILD_FLAGS='-DUPDATE_MANIFEST_URL=\"<url>\"' pio run -e ... -t upload
#
# Then on the device: CFG -> RADIO -> INTERNET must be ON with a home network
# stored, and CFG -> UPDATES -> CHECK NOW. The check raises the association
# itself; nothing needs connecting first.
#
# Usage:
#   ./tools/serve_updates.sh --bump fix             # the everyday one: bump, build, serve
#   ./tools/serve_updates.sh --bump feature         # a feature or a small save bump
#   ./tools/serve_updates.sh --bump major
#   ./tools/serve_updates.sh --bump web             # the 'Pedia bundle instead of firmware
#   ./tools/serve_updates.sh --bump both            # firmware fix + web
#   ./tools/serve_updates.sh                        # publish both at the CURRENT versions
#   ./tools/serve_updates.sh --port 8080
#   ./tools/serve_updates.sh --host 192.168.1.50    # override the autodetected LAN address
#   ./tools/serve_updates.sh --no-serve             # publish into dist/ and stop
#   ./tools/serve_updates.sh --quiet-build          # swallow the compiler output again
#   ./tools/serve_updates.sh --corrupt firmware     # flip a byte -> digest mismatch
#   ./tools/serve_updates.sh --truncate web         # cut the tail -> short download
#   ./tools/serve_updates.sh --skip-assets          # rare: assets/ untouched
#
# `patch`/`minor` still work as names for `fix`/`feature`.
#
# The remaining owed failure path, which needs no flag: start a download on the
# device, then Ctrl-C this server mid-transfer.
#
# What this does NOT do: run the native gates, or touch the device. Publishing
# answers "could a device install this," not "is it correct" — run the gates
# yourself first, same as always.

set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

usage() { sed -n '2,/^set -euo/p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//; /^set -euo/d'; }

PORT=8000
HOST=""
BUMP=""
DAMAGE=""          # corrupt|truncate
DAMAGE_TARGET=""   # firmware|web
SERVE=1
SKIP_ASSETS=0
BUILD_V=1          # stream the compiler by default; --quiet-build swallows it
FW_ENV="${FW_ENV:-waveshare_s3_154}"

die() { echo "error: $*" >&2; exit 1; }

while [ $# -gt 0 ]; do
    case "$1" in
        --port)        PORT="${2:?--port needs a number}"; shift 2 ;;
        --host)        HOST="${2:?--host needs an address}"; shift 2 ;;
        --env)         FW_ENV="${2:?--env needs a PlatformIO env}"; shift 2 ;;
        --bump)        BUMP="${2:?--bump needs fix|feature|major|web|both}"; shift 2 ;;
        --quiet-build) BUILD_V=0; shift ;;
        --corrupt)     DAMAGE=corrupt;  DAMAGE_TARGET="${2:?--corrupt needs firmware|web}"; shift 2 ;;
        --truncate)    DAMAGE=truncate; DAMAGE_TARGET="${2:?--truncate needs firmware|web}"; shift 2 ;;
        --no-serve)    SERVE=0; shift ;;
        --skip-assets) SKIP_ASSETS=1; shift ;;
        -h|--help)     usage; exit 0 ;;
        *) die "unknown flag: $1 (try --help)" ;;
    esac
done

case "$DAMAGE_TARGET" in
    ""|firmware|web) ;;
    *) die "--corrupt/--truncate take firmware or web, not '$DAMAGE_TARGET'" ;;
esac

# Validate the bump word HERE, not when the bump runs: codegen and a full firmware
# build sit between the two, and finding out about a typo after them is a minute
# of waiting for an error that was knowable at the prompt.
case "$BUMP" in
    ""|fix|patch|feature|minor|major|web|both) ;;
    *) die "--bump takes fix, feature, major, web or both, not '$BUMP'" ;;
esac

# --- 1. sprite/palette codegen (assets/ -> src/generated/) ------------------
if [ "$SKIP_ASSETS" -eq 1 ]; then
    echo ">> [1/5] skipping gen_assets.py (--skip-assets)"
else
    echo ">> [1/5] gen_assets.py — assets/**.png -> src/generated/"
    python3 tools/gen_assets.py >/dev/null
fi

# --- 2. version bump (opt-in) ----------------------------------------------
# The firmware's numbers live in include/version.h and the bundle's in
# web/VERSION; both are edited in place, because a published artifact must claim
# the version its own build reports — a bump passed only to the publisher would
# describe an image that disagrees with itself.
#
# Both artifacts pack their version the same way — MAJOR * 100000 + MINOR * 1000 +
# PATCH, mirroring include/version.h — so `fix` has three digits to spend and a
# stretch between features holds a thousand publishes. Roll either field over and
# the code would carry into the one above it, which is why this refuses instead.
pack_code() { echo $(( $1 * 100000 + $2 * 1000 + $3 )); }

check_range() {  # $1=maj $2=min $3=pat, named for the error
    [ "$2" -lt 100 ] || die "MINOR $2 is out of range (0..99) — bump major"
    [ "$3" -lt 1000 ] || die "PATCH $3 is out of range (0..999) — bump feature"
}

# `fix`/`feature` are the words the parts are called; patch/minor still answer to
# them, since that is what the file's macros are named. Returns non-zero rather
# than dying: it runs in a command substitution, where an exit would only end the
# subshell and let the caller carry on with an empty part.
normalise_part() {
    case "$1" in
        fix|patch)     echo patch ;;
        feature|minor) echo minor ;;
        major)         echo major ;;
        *) return 1 ;;
    esac
}

bump_firmware() {  # $1 = fix|feature|major (or their patch/minor aliases)
    local part maj min pat was
    part=$(normalise_part "$1") || die "--bump takes fix, feature or major, not '$1'"
    maj=$(sed -n 's/^#  define FW_VERSION_MAJOR *//p' include/version.h)
    min=$(sed -n 's/^#  define FW_VERSION_MINOR *//p' include/version.h)
    pat=$(sed -n 's/^#  define FW_VERSION_PATCH *//p' include/version.h)
    was="$maj.$min.$pat (code $(pack_code "$maj" "$min" "$pat"))"
    case "$part" in
        patch) pat=$((pat + 1)) ;;
        minor) min=$((min + 1)); pat=0 ;;
        major) maj=$((maj + 1)); min=0; pat=0 ;;
    esac
    check_range "$maj" "$min" "$pat"
    # The .bak dance is BSD sed's -i, which macOS ships; GNU's bare -i is not portable.
    sed -i.bak -e "s/^#  define FW_VERSION_MAJOR .*/#  define FW_VERSION_MAJOR $maj/" \
               -e "s/^#  define FW_VERSION_MINOR .*/#  define FW_VERSION_MINOR $min/" \
               -e "s/^#  define FW_VERSION_PATCH .*/#  define FW_VERSION_PATCH $pat/" \
               include/version.h && rm -f include/version.h.bak
    echo "   firmware    $was"
    echo "            -> $maj.$min.$pat (code $(pack_code "$maj" "$min" "$pat"))   include/version.h"
}

bump_web() {  # $1 = fix|feature|major (or their aliases); defaults to fix
    # `code` is the monotonic integer a check compares; `version` is what it shows.
    # The code is DERIVED from the parts rather than counted separately, so the
    # number that decides and the string that's displayed can't drift apart.
    local part ver maj min pat was
    part=$(normalise_part "${1:-fix}") || die "--bump takes fix, feature or major, not '$1'"
    ver=$(sed -n 's/^version=//p' web/VERSION)
    maj=${ver%%.*}; min=$(echo "$ver" | cut -d. -f2); pat=${ver##*.}
    was="$ver (code $(sed -n 's/^code=//p' web/VERSION))"
    case "$part" in
        patch) pat=$((pat + 1)) ;;
        minor) min=$((min + 1)); pat=0 ;;
        major) maj=$((maj + 1)); min=0; pat=0 ;;
    esac
    check_range "$maj" "$min" "$pat"
    sed -i.bak -e "s/^code=.*/code=$(pack_code "$maj" "$min" "$pat")/" \
               -e "s/^version=.*/version=$maj.$min.$pat/" \
        web/VERSION && rm -f web/VERSION.bak
    echo "   web bundle  $was"
    echo "            -> $maj.$min.$pat (code $(pack_code "$maj" "$min" "$pat"))   web/VERSION"
}

if [ -n "$BUMP" ]; then
    echo ">> [2/5] version bump ($BUMP)"
    case "$BUMP" in
        web)  bump_web fix ;;
        both) bump_firmware fix; bump_web fix ;;
        *)    bump_firmware "$BUMP" ;;
    esac
    echo "         (edited in place — commit them with the change they publish)"
else
    echo ">> [2/5] no version bump (--bump fix|feature|major|web|both)"
    fw_now="$(sed -n 's/^#  define FW_VERSION_MAJOR *//p' include/version.h)."
    fw_now+="$(sed -n 's/^#  define FW_VERSION_MINOR *//p' include/version.h)."
    fw_now+="$(sed -n 's/^#  define FW_VERSION_PATCH *//p' include/version.h)"
    echo "         firmware stays $fw_now, web stays $(sed -n 's/^version=//p' web/VERSION)"
    echo "         a device already on either will report UP TO DATE and install nothing"
fi

# --- 3. publish into dist/ --------------------------------------------------
# BASE must be an address the DEVICE can reach, so localhost is never right.
# en0 is the Wi-Fi interface on Apple silicon laptops; the route lookup is the
# fallback for anything else (a dock, a second adapter, a wired Mac).
if [ -z "$HOST" ]; then
    HOST=$(ipconfig getifaddr en0 2>/dev/null || true)
    if [ -z "$HOST" ]; then
        iface=$(route -n get default 2>/dev/null | sed -n 's/.*interface: //p' | head -1)
        [ -n "$iface" ] && HOST=$(ipconfig getifaddr "$iface" 2>/dev/null || true)
    fi
    [ -n "$HOST" ] || die "could not find this machine's LAN address — pass --host <ip>"
fi
BASE="http://$HOST:$PORT"

echo ">> [3/5] make pages BASE=$BASE  (FW_ENV=$FW_ENV, V=$BUILD_V)"
if [ "$BUILD_V" -eq 1 ]; then
    echo "         building firmware — compiler output follows (--quiet-build to hide)"
else
    echo "         building firmware quietly; this is the slow step"
fi
make --no-print-directory pages BASE="$BASE" FW_ENV="$FW_ENV" V="$BUILD_V"

# --- 4. damage injection (opt-in) -------------------------------------------
# Deliberately AFTER the manifest recorded the honest size + digest: the device
# must be promised a good artifact and then handed a bad one. Re-running
# `make manifest-check` here would now FAIL for --truncate (it re-derives size on
# disk) — that failure is the point, so it is not re-run.
if [ -n "$DAMAGE" ]; then
    dist_dir="${DIST:-dist}"
    if [ "$DAMAGE_TARGET" = firmware ]; then
        victim=$(ls -t "$dist_dir"/mal-*.bin | head -1)
    else
        victim=$(ls -t "$dist_dir"/web-*.tar | head -1)
    fi
    [ -n "$victim" ] || die "no $DAMAGE_TARGET artifact in $dist_dir/ to damage"
    case "$DAMAGE" in
        corrupt)
            # One byte, deep enough in that a header check can't catch it — the
            # SHA-256 over the whole artifact is what has to notice.
            size=$(wc -c < "$victim" | tr -d ' ')
            printf '\xFF' | dd of="$victim" bs=1 seek=$((size / 2)) count=1 conv=notrunc 2>/dev/null
            echo ">> [4/5] CORRUPTED $victim (one byte at offset $((size / 2)))"
            echo "         expect: DOWNLOADING -> CHECKING IT ARRIVED WHOLE -> IT ARRIVED"
            echo "         DAMAGED / NOTHING WAS CHANGED"
            ;;
        truncate)
            size=$(wc -c < "$victim" | tr -d ' ')
            keep=$((size / 2))
            # Rewrite rather than shrink in place, so the served length matches the
            # short file and the device sees a download that simply ends early.
            dd if="$victim" of="$victim.short" bs=1 count="$keep" 2>/dev/null
            mv "$victim.short" "$victim"
            echo ">> [4/5] TRUNCATED $victim ($size -> $keep bytes)"
            echo "         expect: THE DOWNLOAD CUT SHORT / NOTHING WAS CHANGED"
            ;;
    esac
else
    echo ">> [4/5] artifacts intact (--corrupt / --truncate to exercise the failure paths)"
fi

# --- 5. serve ----------------------------------------------------------------
# The two rows THIS manifest points at, not everything dist/ has accumulated —
# older builds are left in place on purpose (a device can be moved back to one),
# so listing the directory would misreport what is being published right now.
dist_dir="${DIST:-dist}"

# One line per artifact, read back OUT OF THE MANIFEST rather than from the files
# or the version headers — this is what the device will actually be told, which is
# the only version of the story worth printing. Damage injection deliberately does
# not update these, so a --corrupt run still prints the honest size it promised.
echo
echo "    publishing:"
python3 - "$dist_dir/manifest.json" <<'PY'
import json, sys
m = json.load(open(sys.argv[1]))
for a in m["artifacts"]:
    name = a["url"].rsplit("/", 1)[-1]
    print(f"      {a['id']:<9} {a['version']:<9} code {a['code']:<8}"
          f"{a['size']/1024:8.1f} KB  {a['sha256'][:12]}…  {name}")
PY

cat <<BANNER

    manifest   $BASE/manifest.json
    flasher    http://localhost:$PORT/flash/   (Web Serial needs localhost, not $HOST)

    On the device, once:
      'Pedia -> DEVICE SETTINGS -> UPDATE SOURCE -> $BASE/manifest.json
      (or over USB: PLATFORMIO_BUILD_FLAGS='-DUPDATE_MANIFEST_URL=\\"$BASE/manifest.json\\"' \\
         pio run -e $FW_ENV -t upload)

    Then:
      CFG -> RADIO -> INTERNET   ON, with a home network stored
      CFG -> UPDATES             the FROM host should read $HOST:$PORT
      CFG -> UPDATES -> CHECK NOW

BANNER

if [ "$SERVE" -eq 0 ]; then
    echo ">> [5/5] not serving (--no-serve). To serve later:"
    echo "     (cd ${DIST:-dist} && python3 -m http.server $PORT)"
    exit 0
fi

echo ">> [5/5] serving ${DIST:-dist}/ on port $PORT — Ctrl-C to stop"
echo "         (each device fetch logs below; a completed firmware pull is the proof)"
echo
exec python3 -m http.server "$PORT" --directory "${DIST:-dist}" --bind 0.0.0.0
