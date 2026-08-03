#!/usr/bin/env bash
#
# provision.sh — take a GENUINELY FRESH ESP32-S3 board and a FRESH microSD card
# and leave BOTH in the verified state this project actually runs on. Not "flash
# the firmware": a blank board is missing the custom partition table the save
# depends on, and a blank/exFAT card won't mount at all — so provisioning has
# real prerequisites on both media, and this script checks its own work rather
# than trusting that a command "ran without erroring."
#
# It is a superset of tools/flash.sh (the day-to-day reflash primitive), and
# DELEGATES to it for the build+upload+report step rather than reinventing
# --wait-wake and the partition/save-usage report. What provision.sh adds:
#   • a full chip erase + a forced clean rebuild (both mandatory the first time
#     a custom partition table lands — see the erase/clean notes below),
#   • ASSERTIONS on top of flash.sh's human-readable report: the `malsave` NVS
#     partition must actually be present at the right size, and the device's
#     boot lines must report a healthy save blob (well under capacity) and a
#     working SD round-trip,
#   • the SD side flash.sh only ever touches opt-in: optional FAT32 (re)format
#     with hard guards, staging the /web 'Pedia bundle, and verifying it landed.
#
# Two media, two phases, done in this order because the card physically moves:
#   1) `sd`    — card in a READER on THIS computer. Format (optional) + stage +
#                verify the bundle on the card.
#   2) `board` — card can now be seated in the DEVICE. Erase + clean build +
#                flash + verify partition table + verify boot (incl. that the
#                just-provisioned card mounts on-device).
# `all` runs 1, prompts you to move the card into the device, then runs 2.
#
# Usage:
#   ./tools/provision.sh all --format --device disk4         # blank board + blank card, start to finish
#   ./tools/provision.sh all --dest /Volumes/MALWARIUM       # card already FAT32-formatted
#   ./tools/provision.sh sd --format --device disk4          # just the card (formats + stages)
#   ./tools/provision.sh sd --dest /Volumes/MALWARIUM        # just the card (already FAT32; stage only)
#   ./tools/provision.sh board                               # just the board (blank chip, USB attached)
#   ./tools/provision.sh board --wait-wake                   # re-provision an ALREADY-ALIVE board (light-sleep aware)
#
# Flags (per subcommand; `all` accepts the union):
#   --dest <mount>   Mounted card path for staging (e.g. /Volumes/MALWARIUM).
#                    Required for `sd` WITHOUT --format; ignored with --format
#                    (a format renames the volume MALWARIUM and re-derives it).
#   --format         Reformat the card FAT32 before staging. DESTRUCTIVE — needs
#                    --device and interactive confirmation; refuses internal disks.
#   --device <diskN> Whole-disk id to format (macOS `diskutil list`, e.g. disk4).
#   --port <path>    Serial port for the board upload/verify (default: autodetect).
#   --wait-wake      Pass through to flash.sh AND wrap the erase: retry across the
#                    light-sleep window with a one-time "press a button" prompt.
#                    Only needed to RE-provision a board that's already running
#                    firmware (it light-sleeps the USB-JTAG link); a truly blank
#                    chip has nothing running to sleep, so the first flash won't
#                    need it.
#   --keep-build     Skip the `rm -rf .pio/build/<env>` clean. Don't use this for
#                    a partition-table change — a stale cache silently re-emits
#                    the OLD firmware.bin offset and the board boot-loops.
#   --env <name>     PlatformIO env (default: waveshare_s3_154 — the only target).
#   -h, --help       Show this help.

set -euo pipefail

ENV="waveshare_s3_154"
PORT=""
DEST=""
DEVICE=""
DO_FORMAT=0
WAIT_WAKE=0
KEEP_BUILD=0
IDLE_SLEEP_WAKE_BUDGET_S=150   # mirrors flash.sh: straddle one light-sleep pulse

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
flash_sh="$repo_root/tools/flash.sh"

# The custom table's dedicated save partition (partitions_malwarium.csv). These
# are what the post-flash partition assertion checks the built binary against —
# if a future table edit changes them, update here too.
EXPECT_PART_NAME="malsave"
EXPECT_PART_SIZE="256K"

usage() { sed -n '2,/^set -euo/p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//; /^set -euo/d'; }
say()  { echo ">> $*"; }
die()  { echo "provision.sh: $*" >&2; exit 1; }

# ---------------------------------------------------------------------------
# arg parse: first token is the subcommand, the rest are flags
# ---------------------------------------------------------------------------
[ $# -ge 1 ] || { usage; exit 2; }
SUB="$1"; shift
case "$SUB" in sd|board|all) ;; -h|--help|help) usage; exit 0 ;; *) die "unknown subcommand '$SUB' (sd|board|all; --help)";; esac

while [ $# -gt 0 ]; do
    case "$1" in
        --dest)       DEST="${2:-}"; shift 2 ;;
        --dest=*)     DEST="${1#*=}"; shift ;;
        --device)     DEVICE="${2:-}"; shift 2 ;;
        --device=*)   DEVICE="${1#*=}"; shift ;;
        --format)     DO_FORMAT=1; shift ;;
        --port)       PORT="${2:-}"; shift 2 ;;
        --port=*)     PORT="${1#*=}"; shift ;;
        --wait-wake)  WAIT_WAKE=1; shift ;;
        --keep-build) KEEP_BUILD=1; shift ;;
        --env)        ENV="${2:-}"; shift 2 ;;
        --env=*)      ENV="${1#*=}"; shift ;;
        -h|--help)    usage; exit 0 ;;
        *) die "unknown argument '$1' (try --help)" ;;
    esac
done

gen_part_py() {
    find "$HOME/.platformio/packages/framework-arduinoespressif32" \
        -maxdepth 3 -name gen_esp32part.py 2>/dev/null | head -1
}

# ---------------------------------------------------------------------------
# SD phase
# ---------------------------------------------------------------------------

# diskutil-guarded FAT32 reformat. The firmware mounts read-only-ish via
# esp_vfs_fat_sdmmc_mount() with format_if_mount_failed=false (sd_esp32.h) — it
# NEVER formats the card itself and it CANNOT mount exFAT, so a fresh card that
# ships exFAT (common >32GB) has to be brought to FAT32 here first. Whole-disk,
# external-only, with an explicit typed confirmation — wiping the wrong disk is
# unrecoverable.
sd_format() {
    [ -n "$DEVICE" ] || die "--format needs --device <diskN> (see: diskutil list)"
    local dev="${DEVICE#/dev/}"                       # accept 'disk4' or '/dev/disk4'
    case "$dev" in disk[0-9]) ;; disk[0-9][0-9]) ;; *) die "--device must be a WHOLE disk like disk4, not a slice or volume";; esac
    command -v diskutil >/dev/null || die "diskutil not found — the --format path is macOS-only (use --dest on an already-FAT32 card, or format it yourself)"

    local info location removable virtual
    info="$(diskutil info "/dev/$dev" 2>/dev/null)" || die "/dev/$dev is not a disk diskutil knows about"
    # Field names vary by macOS version: modern builds report 'Device Location'
    # (Internal/External) + 'Removable Media' (Fixed/Removable) rather than a
    # single 'Internal:' line. Read both and require the disk to look external by
    # EITHER signal; 'Virtual' must be No. Anything ambiguous fails closed —
    # wiping the wrong disk is unrecoverable and there is deliberately no override.
    location="$(printf  '%s\n' "$info" | awk -F': *' '/Device Location:/{print $2; exit}')"
    removable="$(printf '%s\n' "$info" | awk -F': *' '/Removable Media:/{print $2; exit}')"
    virtual="$(printf   '%s\n' "$info" | awk -F': *' '/^ *Virtual:/{print $2; exit}')"
    [ "$virtual" = "No" ] || die "/dev/$dev reports Virtual='$virtual' — refusing to format a virtual disk"
    if [ "$location" != "External" ] && [ "$removable" != "Removable" ]; then
        die "/dev/$dev looks non-external (Device Location='$location', Removable Media='$removable') — refusing. Only external/removable cards can be formatted here."
    fi

    echo
    echo "About to ERASE and reformat FAT32 (this destroys everything on it):"
    diskutil list "/dev/$dev" || true
    echo
    printf "Type the device id '%s' to confirm the wipe: " "$dev"
    local reply; read -r reply
    [ "$reply" = "$dev" ] || die "confirmation did not match ('$reply' != '$dev') — aborting, nothing was erased"

    say "formatting /dev/$dev as FAT32 (volume MALWARIUM)…"
    # FAT32 personality + MBR — the layout SD_MMC expects. Volume name is the
    # FAT 11-char uppercase label; it becomes the /Volumes mount point.
    diskutil eraseDisk FAT32 MALWARIUM MBRFormat "/dev/$dev"
    DEST="/Volumes/MALWARIUM"
    say "formatted; mounted at $DEST"
}

# Confirm the destination really is a FAT filesystem before staging — the one
# on-card property the firmware can't work around. An exFAT card copies fine
# here but silently fails to mount on the device (sd_esp32.h lastError()).
sd_verify_fs() {
    command -v diskutil >/dev/null || { say "diskutil unavailable — skipping FAT32 check (ensure the card is FAT32, not exFAT)"; return 0; }
    local personality
    personality="$(diskutil info "$DEST" 2>/dev/null | awk -F': *' '/File System Personality:/{print $2; exit}')"
    case "$personality" in
        *FAT32*|*FAT16*|*MS-DOS*) say "filesystem: $personality (FAT — good)" ;;
        *xFAT*|*ExFAT*|*exFAT*)   die "card is $personality — the device CANNOT mount exFAT. Reformat FAT32 (re-run with --format --device diskN)." ;;
        "" ) say "couldn't read filesystem personality for $DEST — proceeding, but confirm it's FAT32 not exFAT" ;;
        *  ) say "filesystem: $personality — proceeding, but the device needs FAT32/FAT16 (not exFAT)" ;;
    esac
}

do_sd() {
    if [ "$DO_FORMAT" -eq 1 ]; then
        sd_format
    else
        [ -n "$DEST" ] || die "sd needs --dest <mount> (or --format --device diskN to format one)"
    fi
    [ -d "$DEST" ] || die "--dest '$DEST' is not a mounted directory. Insert the card in a READER on THIS machine (not the device) and pass its mount point."
    sd_verify_fs

    # Stage the served bundle via the existing Make target (regenerates
    # pedia_data.js first, so the staged copy is never stale) — the same path
    # flash.sh --pedia uses. Files land at $DEST/web/… where ap_server.h serves
    # them from /sdcard/web/<path>.
    say "staging /web 'Pedia bundle -> $DEST/web/"
    make -C "$repo_root" pedia-sd DEST="$DEST"

    # Verify the bundle actually landed — not just that make exited 0.
    local missing=0 f
    for f in web/index.html web/style.css web/app.js web/data/pedia_data.js; do
        if [ ! -f "$DEST/$f" ]; then echo "   MISSING: $DEST/$f" >&2; missing=1; fi
    done
    local nassets
    nassets="$(find "$DEST/web/assets" -type f 2>/dev/null | wc -l | tr -d ' ')"
    [ "${nassets:-0}" -gt 0 ] || { echo "   MISSING: $DEST/web/assets/ is empty" >&2; missing=1; }
    [ "$missing" -eq 0 ] || die "SD bundle verification failed — the card is not fully provisioned"
    say "SD verified: index.html + style.css + app.js + data/pedia_data.js + assets/ ($nassets files)"
    say "eject the card and move it into the DEVICE before flashing (the on-device SD check needs it seated)."
}

# ---------------------------------------------------------------------------
# board phase
# ---------------------------------------------------------------------------

# Run a pio flash-touching command, optionally retrying across the light-sleep
# window (the flash.sh --wait-wake pattern, applied here to `erase` which flash.sh
# doesn't cover). A truly blank chip connects on the first try; an already-alive
# board may be light-sleeping its USB-JTAG link until a button wakes it.
run_flashcmd() {   # args: pio subcommand words
    if [ "$WAIT_WAKE" -eq 0 ]; then
        ( cd "$repo_root" && pio "$@" ); return
    fi
    say "if the board's screen is asleep, press any button on it (a button wake holds USB up far longer than the timer pulse)."
    local deadline attempt=0
    deadline=$(( $(date +%s) + IDLE_SLEEP_WAKE_BUDGET_S ))
    until ( cd "$repo_root" && pio "$@" ); do
        attempt=$(( attempt + 1 ))
        [ "$(date +%s)" -lt "$deadline" ] || die "no wake pulse within ${IDLE_SLEEP_WAKE_BUDGET_S}s (${attempt} attempts) — giving up"
        say "attempt ${attempt} missed the wake window, retrying…"
    done
}

# Assert the just-built partition binary carries the dedicated save partition.
# Decodes the REAL .bin (not the CSV) — a stale build cache can emit an old table
# even after the CSV changed, and that mismatch is exactly what boot-loops the board.
board_verify_partition() {
    local bin="$repo_root/.pio/build/$ENV/partitions.bin"
    local tool; tool="$(gen_part_py)"
    [ -f "$bin" ] || die "no partitions.bin at $bin — did the build run?"
    [ -n "$tool" ] || { say "gen_esp32part.py not found — skipping partition assertion (flash.sh already printed the table)"; return 0; }
    local table; table="$(python3 "$tool" "$bin" 2>/dev/null)"
    echo "$table" | sed 's/^/   /'
    echo "$table" | awk -F, -v n="$EXPECT_PART_NAME" -v s="$EXPECT_PART_SIZE" '
        $1==n && $3=="nvs" && $5==s {found=1}
        END{exit found?0:1}' \
        || die "partition table is MISSING '$EXPECT_PART_NAME' ($EXPECT_PART_SIZE nvs). The save has nowhere to live — the board would silently drop writes. Re-run WITHOUT --keep-build (a clean rebuild) so the custom table is emitted."
    say "partition OK: '$EXPECT_PART_NAME' present ($EXPECT_PART_SIZE nvs)"
}

# Best-effort-but-retried capture of the boot lines, asserting health:
#   [save] … blob=X/YB (Z%)   → Z% must be < 100 and no "FAILED" (the NVS-full bug)
#   [sd] mounted …MB; round-trip OK   → the card provisioned above actually mounts
# DTR must be asserted or the firmware sits in its host-wait boot path and never
# proceeds (native USB-JTAG; plain cat won't trigger it). A reset re-enumerates
# USB briefly, so we open the port, THEN pulse a reset, THEN read. If the board is
# asleep/unreachable this degrades to a warning — flash.sh already surfaced the
# [save] line for the human, and the partition assertion above is the hard gate.
board_verify_boot() {
    local port="${PORT:-$(awk -v e="[env:$ENV]" '
        $0==e{ineq=1} /^\[/&&$0!=e{ineq=0} ineq&&$1=="monitor_port"{print $3; exit}' \
        "$repo_root/platformio.ini")}"
    if [ -z "$port" ] || ! python3 -c "import serial" >/dev/null 2>&1; then
        say "no serial port / pyserial — skipping on-device boot assertion (check 'pio device monitor' for [save] blob<100% and [sd] round-trip OK)"
        return 0
    fi
    say "verifying boot lines on $port…"
    # `|| rc=$?` captures python's exit without `set -e` aborting on a non-zero
    # (an unhealthy line reports rc 2, which we translate to a hard fail below).
    local rc=0
    python3 - "$port" <<'PYEOF' || rc=$?
import serial, sys, time
port = sys.argv[1]
save_ok = sd_ok = None
save_line = sd_line = ""
try:
    ser = serial.Serial(port, 115200, timeout=0.3)
    ser.dtr = True                       # firmware waits for a host-connected Serial
    # Pulse a reset so we capture a fresh boot. On the S3 USB-Serial/JTAG CDC,
    # RTS is wired to EN — assert briefly to reset into the running app (keep DTR
    # asserted so it does NOT drop into download mode).
    try:
        ser.rts = True; time.sleep(0.15); ser.rts = False
    except Exception:
        pass                             # some CDC stacks ignore it; the post-upload reset may still be in flight
    end = time.time() + 12
    while time.time() < end and (save_ok is None or sd_ok is None):
        line = ser.readline().decode(errors="replace").strip()
        if not line:
            continue
        if line.startswith("[save]"):
            save_line = line
            pct = None
            for tok in line.replace("("," ").replace(")"," ").split():
                if tok.endswith("%"):
                    try: pct = int(tok[:-1])
                    except ValueError: pass
            save_ok = ("FAILED" not in line) and (pct is not None and pct < 100)
        elif line.startswith("[sd]"):
            sd_line = line
            sd_ok = ("round-trip OK" in line)
    ser.close()
except Exception as e:
    print(f"   (serial capture failed: {e})")

rc = 0
if save_line: print("   " + save_line)
else:         print("   (no [save] line captured)")
if sd_line:   print("   " + sd_line)
else:         print("   (no [sd] line captured)")

if save_ok is False:
    print("   !! SAVE UNHEALTHY — blob at/over capacity or a write FAILED (the NVS-full bug). NOT provisioned.")
    rc = 2
elif save_ok is None:
    print("   .. couldn't confirm the save line this time (best-effort) — power-cycle and check 'pio device monitor'.")
if sd_ok is False:
    print("   !! SD did NOT round-trip on-device — the card isn't mounting (exFAT? bad seat?). Re-provision the card.")
    rc = 2 if rc == 0 else rc
elif sd_ok is None and rc == 0:
    print("   .. couldn't confirm the [sd] line (best-effort) — seat the card and check the monitor.")
sys.exit(rc)
PYEOF
    # rc 2 = a line was captured AND reported unhealthy → hard fail; anything
    # else (couldn't capture) stays a warning, per the best-effort contract.
    [ "$rc" -ne 2 ] || die "on-device verification FAILED (see the flagged line above)"
}

do_board() {
    command -v pio >/dev/null || die "pio (PlatformIO CLI) not found on PATH"
    [ -x "$flash_sh" ] || die "expected $flash_sh (this script builds on flash.sh)"

    # Warn if the staged 'Pedia data would be stale vs. the content tables. Not
    # fatal for a board-only run (the card may just not be re-staged); `sd`/`all`
    # always regenerate it fresh.
    make -C "$repo_root" pedia-check >/dev/null 2>&1 || say "note: web/data/pedia_data.js looks STALE vs. content tables — run './tools/provision.sh sd …' (or 'make pedia') to re-stage the card."

    # A partition-table change only reliably takes effect from a clean build:
    # a stale cache keeps emitting the OLD firmware.bin offset even after
    # partitions.bin is regenerated, which boot-loops the board. Erase clears any
    # factory/leftover flash AND any stale non-blank NVS pages before the new
    # table lands. Both are cheap insurance and both are load-bearing here.
    if [ "$KEEP_BUILD" -eq 0 ]; then
        say "clean: rm -rf .pio/build/$ENV (a partition change needs a non-cached rebuild)"
        rm -rf "$repo_root/.pio/build/$ENV"
    fi
    say "erasing entire flash (pio run -e $ENV -t erase)"
    run_flashcmd run -e "$ENV" -t erase

    # Delegate build + upload + the capacity/save-usage report to flash.sh.
    say "flashing via flash.sh"
    local fargs=(--env "$ENV")
    [ -n "$PORT" ] && fargs+=(--port "$PORT")
    [ "$WAIT_WAKE" -eq 1 ] && fargs+=(--wait-wake)
    "$flash_sh" "${fargs[@]}"

    # Now the assertions flash.sh's human-readable report stops short of.
    board_verify_partition
    board_verify_boot
    say "board provisioned + verified."
}

# ---------------------------------------------------------------------------
case "$SUB" in
    sd)    do_sd ;;
    board) do_board ;;
    all)
        do_sd
        echo
        say "SD done. Eject the card, seat it in the DEVICE, connect the board over USB."
        printf ">> press Enter when the card is in the device and it's plugged in… "
        read -r _
        do_board
        ;;
esac
say "provision: done."
