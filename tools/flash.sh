#!/usr/bin/env bash
#
# flash.sh — flash the Malwarium firmware, and OPTIONALLY re-stage the web
# 'Pedia bundle onto the device's SD card in the same run.
#
# Two things live on two different media: the firmware goes into the ESP32's
# internal flash over USB, while the 'Pedia site (web/) lives on the microSD
# card. They change on opposite cadences — you reflash often, but the SD bundle
# only drifts when web/ or the content tables change — so SD staging is OPT-IN
# behind --pedia and never touches the card on a plain flash.
#
# The catch is physical: staging needs the card mounted in a READER on this
# computer (not sitting in the device), so the workflow when you use --pedia is:
#   card in reader -> ./tools/flash.sh --pedia /Volumes/MALWARIUM -> move card
#   back into the device. The firmware flash itself doesn't care where the card
#   is, so it's fine to do both in one go.
#
# Usage:
#   ./tools/flash.sh                              # just flash the S3 firmware
#   ./tools/flash.sh --pedia /Volumes/MALWARIUM   # stage 'Pedia to SD, then flash
#   ./tools/flash.sh --pedia /Volumes/MALWARIUM --no-upload
#                                                 # ONLY re-sync the SD 'Pedia
#                                                 # (no reflash — for when an
#                                                 # update desynced the site)
#   ./tools/flash.sh --env waveshare_s3_154_bringup --port /dev/cu.usbmodemXXXX
#   ./tools/flash.sh --wait-wake
#
# Flags:
#   --pedia <DEST>   Stage web/ onto the SD card mounted at <DEST> before
#                    flashing (runs `make pedia-sd`, which regenerates
#                    pedia_data.js first). Files land at <DEST>/web/... .
#   --no-upload      Skip the firmware flash. Only meaningful with --pedia:
#                    "just re-sync the 'Pedia, I already flashed."
#   --env <name>     PlatformIO env to flash (default: waveshare_s3_154).
#   --port <path>    Serial port for the upload (default: PlatformIO autodetect).
#   --wait-wake      Retry the upload across IDLE_SLEEP_WAKE_BUDGET_S seconds
#                    instead of failing on the first "No serial data received".
#                    While the device is screen-asleep + radio-quiet it light-
#                    sleeps the SoC (config.h IDLE_SLEEP_FALLBACK_MS) and the
#                    USB-Serial/JTAG peripheral goes unresponsive with it.
#                    The periodic timer-wakeup pulse alone isn't a reliable
#                    target — loop() finds nothing changed and re-sleeps
#                    almost immediately, too short a window for esptool's
#                    handshake. The loop prints a one-time prompt to press a
#                    button on the device instead: a real button wake resets
#                    the screen's own inactivity timer, holding USB up for the
#                    30s+ that follows — that's the window this is really
#                    retrying into. Each failed esptool connect attempt already
#                    spends ~14s in its own internal retry loop, so looping the
#                    upload comfortably covers the time it takes to notice the
#                    prompt and reach over to press a button.
#   -h, --help       Show this help.

set -euo pipefail

ENV="waveshare_s3_154"
PORT=""
PEDIA_DEST=""
DO_UPLOAD=1
WAIT_WAKE=0
# Comfortably longer than IDLE_SLEEP_FALLBACK_MS (config.h, 120s today) so a
# --wait-wake retry loop is guaranteed to straddle at least one wake pulse
# even if the first attempt starts right after one.
IDLE_SLEEP_WAKE_BUDGET_S=150

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

usage() { sed -n '2,/^set -euo/p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//; /^set -euo/d'; }

while [ $# -gt 0 ]; do
    case "$1" in
        --pedia)     PEDIA_DEST="${2:-}"; shift 2 ;;
        --pedia=*)   PEDIA_DEST="${1#*=}"; shift ;;
        --no-upload) DO_UPLOAD=0; shift ;;
        --env)       ENV="${2:-}"; shift 2 ;;
        --env=*)     ENV="${1#*=}"; shift ;;
        --port)      PORT="${2:-}"; shift 2 ;;
        --port=*)    PORT="${1#*=}"; shift ;;
        --wait-wake) WAIT_WAKE=1; shift ;;
        -h|--help)   usage; exit 0 ;;
        *) echo "flash.sh: unknown argument '$1' (try --help)" >&2; exit 2 ;;
    esac
done

if [ "$DO_UPLOAD" -eq 0 ] && [ -z "$PEDIA_DEST" ]; then
    echo "flash.sh: --no-upload with no --pedia leaves nothing to do." >&2
    exit 2
fi

# --- optional 'Pedia staging (opt-in) --------------------------------------
if [ -n "$PEDIA_DEST" ]; then
    if [ ! -d "$PEDIA_DEST" ]; then
        echo "flash.sh: --pedia DEST '$PEDIA_DEST' is not a mounted directory." >&2
        echo "         Insert the SD card into a reader on THIS machine (not the" >&2
        echo "         device) and pass its mount point, e.g. /Volumes/MALWARIUM." >&2
        exit 2
    fi
    echo ">> staging web 'Pedia bundle -> $PEDIA_DEST/web/"
    make -C "$repo_root" pedia-sd DEST="$PEDIA_DEST"
fi

# --- firmware flash ---------------------------------------------------------
if [ "$DO_UPLOAD" -eq 1 ]; then
    upload_args=(run -e "$ENV" -t upload)
    [ -n "$PORT" ] && upload_args+=(--upload-port "$PORT")
    echo ">> flashing $ENV${PORT:+ (port $PORT)}"
    if [ "$WAIT_WAKE" -eq 1 ]; then
        echo ">> looking for the device — make sure it's plugged in, and if the" \
             "screen is asleep, press any button on it to wake it (a button wake" \
             "holds the USB link up far longer than the light-sleep timer pulse" \
             "this loop would otherwise be waiting on)."
        deadline=$(( $(date +%s) + IDLE_SLEEP_WAKE_BUDGET_S ))
        attempt=0
        until ( cd "$repo_root" && pio "${upload_args[@]}" ); do
            attempt=$(( attempt + 1 ))
            if [ "$(date +%s)" -ge "$deadline" ]; then
                echo ">> --wait-wake: no wake pulse landed within ${IDLE_SLEEP_WAKE_BUDGET_S}s" \
                     "(${attempt} attempts) — giving up." >&2
                exit 1
            fi
            echo ">> attempt ${attempt} missed the wake window, retrying..."
        done
    else
        ( cd "$repo_root" && pio "${upload_args[@]}" )
    fi
fi

# --- flash-space report (best-effort, never fails the run) -----------------
# The NOT_ENOUGH_SPACE regression (docs — the save partition silently filling
# with no warning until a write started failing) is why this exists: surface
# both capacities at flash time instead of finding out the hard way later.
if [ "$DO_UPLOAD" -eq 1 ]; then
    partitions_bin="$repo_root/.pio/build/$ENV/partitions.bin"
    gen_part_py="$(find "$HOME/.platformio/packages/framework-arduinoespressif32" \
                        -maxdepth 3 -name gen_esp32part.py 2>/dev/null | head -1)"
    if [ -f "$partitions_bin" ] && [ -n "$gen_part_py" ]; then
        echo ">> flash partitions (capacity):"
        python3 "$gen_part_py" "$partitions_bin" 2>/dev/null | sed -n '/^#/!p; /^# Name/p' | sed 's/^/   /'
    fi

    # The static table above only gives CAPACITY. Actual save USAGE is runtime
    # data (Game::saveBlobSizeBytes) that only the device itself can report, via
    # the "[save] ... blob=X/YB" boot line (main.cpp) — grab it with a short,
    # best-effort serial listen. DTR must be asserted (plain `cat`/shell
    # redirection on this native-USB-JTAG port won't trigger it) or the device
    # sits in its host-wait boot path instead of proceeding, so this needs
    # pyserial specifically; skip quietly if it isn't available rather than
    # failing a flash over a diagnostic nicety.
    save_port="${PORT:-$(awk -v e="[env:$ENV]" '
        $0==e {ineq=1} /^\[/ && $0!=e {ineq=0}
        ineq && $1=="monitor_port" {print $3; exit}
    ' "$repo_root/platformio.ini")}"
    if [ -n "$save_port" ] && python3 -c "import serial" >/dev/null 2>&1; then
        echo ">> checking current save usage (${save_port})..."
        python3 - "$save_port" <<'PYEOF' || true
import serial, sys, time
port = sys.argv[1]
end = time.time() + 6
found = False
try:
    ser = serial.Serial(port, 115200, timeout=0.3)
    ser.dtr = True
    while time.time() < end and not found:
        line = ser.readline().decode(errors="replace").strip()
        if line.startswith("[save]"):
            print("   " + line)
            found = True
    ser.close()
except Exception:
    pass
if not found:
    print("   (didn't catch the boot line this time — check the serial monitor directly)")
PYEOF
    fi
fi

if [ -n "$PEDIA_DEST" ]; then
    echo ">> done. Eject the card and move it back into the device before the"
    echo "   'Pedia AP can serve the updated bundle."
else
    echo ">> done."
fi
