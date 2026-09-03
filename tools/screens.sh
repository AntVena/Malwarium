#!/usr/bin/env bash
# screens.sh — render the screen catalogue to PNGs for a visual sweep.
#
#   ./tools/screens.sh [outdir]      # default: build/screens
#
# Why this exists: the native tier can prove a STRING fits a budget, but not that two
# things drawn on the same line miss each other, and not that a panel's copy is being
# cut. Those show up the moment you look at the frame, and nowhere else — a font or a
# layout constant moves, thirty screens still "render", and a handful are quietly
# overprinting. Run this after any change to the grid (core/ui/layout.h), the font, or
# a shared widget, and look at the sheet.
#
# It is a LOOKING tool, not a gate: there are no baselines to diff and it never fails.
# The screenshot-regression tier is a separate, deferred decision (docs/MASTER_TODO.md).
#
# Each line below is one scene; the flags are dump_frame's own (see its header for the
# full vocabulary). A scene that renders the idle habitat means its flags didn't match
# and the line needs fixing — the script says so rather than leaving you to spot it in
# a sheet of twenty.
set -euo pipefail

cd "$(dirname "$0")/.."
OUT="${1:-build/screens}"
DUMP=build/dump_frame
[ -x "$DUMP" ] || { echo "build $DUMP first (./tools/gates.sh --native)"; exit 1; }

mkdir -p "$OUT"
rm -f "$OUT"/*.ppm
"$DUMP" 3 "$OUT/_idle.ppm" >/dev/null

SCENES=(
  "expl|expl"                       "expl_inside|expl inside"
  "dock|dock"                       "dock_round2|dock fight"
  "dock_scout|dock scout"           "dock_brief|dock brief"
  "stat|stat"                       "stat_species|stat species"
  "stat_buffs|stat buffs armbuffs"  "stat_buffs_scrolled|stat buffs armbuffs scroll:1"
  "stat_loadout|stat loadout"
  "stat_loadout_full|stat loadout fullkit pet:wire_heir"
  "stat_loadout_scrolled|stat loadout fullkit pet:wire_heir scroll:1"
  "stat_species_long|stat species pet:wire_heir"
  "stat_log|stat log"               "items|items"
  "item_detail|items detail"        "arcade|arcade"
  "arcade_cabinet|arcade cabinet"   "arcade_board|arcade cabinet play"
  "arcade_result|arcade cabinet play result"
  "cryptogram|cryptogram"           "cryptogram_part|cryptogram open:4"
  "cryptogram_hold|cryptogram open:2 take"
  "cryptogram_win|cryptogram win"   "cryptogram_lost|cryptogram lose"
  "clutch|clutch aim"               "clutch_win|clutch win"
  "clutch_lose|clutch lose"
  "loadout|loadout"
  "mods|mods"                       "mod_detail|mods detail"
  "mod_picker|mods picker modlocked"
  "train|train"
  "move_picker|train trainpicker focus"
  "move_detail|train movedetail"    "maint|maint"
  "maint_detail|maint detail"       "arch|arch"
  "arch_detail|arch detail"
  "arch_scroll|arch rackfull row:8" "cfg|cfg"
  "cfg_sysinfo|cfg sysinfo"         "cfg_tag|cfg tag"
  "cfg_titles|cfg titles"           "cfg_radio|cfg radio all"
  "cfg_audit|cfg audit"             "cfg_updates|cfg updates ready found"
  "cfg_confirm|cfg updates ready confirm"
  "explore_cachefind|explore cachefind"
  "hacker_profile|hacker profile"   "hacker_shop|hacker shop hub"
  "hacker_profile_wide|hacker profile decorated"
  "hacker_vault|hacker vault"       "hacker_crew|hacker crew joined"
  "hacker_crew_unset|hacker crew unset"
  "hacker_crew_side|hacker crew joined blue"
  "hacker_crew_scroll|hacker crew joined blue row:3"
  "hacker_crew_detail|hacker crew red row:1 detail"
  "hacker_crew_netpick|hacker crew netpick"
  "hacker_merge|hacker merge recipes stock"
  "shibboleth|shibboleth"           "shibboleth_half|shibboleth sigils:13"
  "shibboleth_fluent|shibboleth sigils:24"
  # FX_SWARM: the guardian's body, one scene per FlockMood — a swarm is judged on whether
  # the moods read as different creatures, and that is a looking question, not a gate one.
  # The `beats` count runs the flock to its shape before the frame is taken.
  "shibboleth_hail|shibboleth hail beats:70"
  "shibboleth_hail_half|shibboleth hail sigils:13 beats:70"
  "shibboleth_verdict|shibboleth verdict beats:70"
  "shibboleth_verdict_fluent|shibboleth verdict sigils:24 beats:70"
  "shibboleth_refused|shibboleth refused beats:70"
  "shibboleth_boon|shibboleth boon sigils:26 beats:70"
  "shibboleth_verdict_wrong|shibboleth verdict wrong sigils:13 beats:70"
  "shop|shop"                       "encounter|encounter"
  "combat|combat"                   "postencounter|postencounter"
  "combat_stats|combat stats"       "combat_kit|combat kit"
  "combat_bands|combat override crew pantry"
  "combat_band_rows|combat override crew pantry band:1"
  "combat_wide|combat pet:baitracuda"
  "lockout|lockout"
  "decryption|decryption"               "decryption_rows|decryption rows"
  "decryption_lost|decryption lost"
  "isolation|isolation steps:600"   "isolation_crash|isolation crash"
  "worm_egg|isolation crash bank"
  "ach_egg_line|acheggline"
  "chroma|chroma wear"                "chroma_half|chroma half"
  "chroma_spotted|chroma spotted"     "chroma_clean|chroma clean"
  "meta_egg|chroma clean bank"
  "worm_home|pet:nodeatode"           "worm_script|pet:rootgrub"
)

n=0
for scene in "${SCENES[@]}"; do
  name="${scene%%|*}"; args="${scene#*|}"
  n=$((n + 1))
  # shellcheck disable=SC2086  # the flags are meant to word-split
  "$DUMP" 3 "$OUT/$(printf %02d $n)_$name.ppm" $args >/dev/null
  if cmp -s "$OUT/$(printf %02d $n)_$name.ppm" "$OUT/_idle.ppm"; then
    echo "  no-op: '$name' [$args] rendered the idle habitat — its flags matched nothing"
  fi
done

python3 - "$OUT" <<'PY'
import glob, os, sys
try:
    from PIL import Image, ImageDraw
except ImportError:
    sys.exit("contact sheet needs Pillow; the .ppm frames are in " + sys.argv[1])
out = sys.argv[1]
fs = sorted(glob.glob(os.path.join(out, "[0-9]*.ppm")))
ims = [(os.path.basename(f)[3:-4], Image.open(f)) for f in fs]
w, h = ims[0][1].size
cols = 5
rows = (len(ims) + cols - 1) // cols
sheet = Image.new("RGB", (w * cols + 10 * (cols - 1), (h + 14) * rows), (12, 12, 16))
d = ImageDraw.Draw(sheet)
for i, (n, im) in enumerate(ims):
    x, y = (i % cols) * (w + 10), (i // cols) * (h + 14)
    sheet.paste(im, (x, y + 14))
    d.text((x + 2, y + 2), n, fill=(150, 160, 175))
sheet.save(os.path.join(out, "contact_sheet.png"))
print(f"{len(ims)} screens -> {out}/contact_sheet.png")
PY
