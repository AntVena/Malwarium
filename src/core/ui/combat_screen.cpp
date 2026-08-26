#include "core/ui/combat_screen.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "tunables.h"        // kLevelDmgReduceMaxPct — the never-immune defence clamp
#include "core/model/combat.h"
#include "core/render/absorb.h"
#include "core/render/camo.h"
#include "core/render/canvas.h"
#include "core/render/shred.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"
#include "core/ui/worm_replicas.h"
#include "generated/assets.h"
#include "core/ui/theme.h"

namespace mal {

namespace {


// Player Health zone from the remaining fraction (vitality polarity): cool
// -> warn -> hot+pulse as it empties, the same danger language as a Critical vital.
Zone healthZone(int health, int maxHealth) {
    if (maxHealth <= 0) return Zone::Critical;
    const int pct = health * 100 / maxHealth;
    if (pct <= 20) return Zone::Critical;
    if (pct <= 50) return Zone::Caution;
    return Zone::Ok;
}

// The stage floor both fighters stand on, and the band above it they occupy. The shelf
// sits low so a tall Daemon (up to 64 logical / 112 active px) has headroom before it
// reaches the rival's Health row, at the cost of the player block below it — that block
// is packed tight against the hint band instead of leaving it mid-screen.
constexpr int kSpriteShelf = kCombatSpriteShelf;   // the panel's geometry names it too

// The two Health gauges' shared column. Both fighters' rows are laid out against these
// rather than each measuring its own caption, so the bars start on one x, run to one
// width, and put their numerics on one right edge — which is what lets an operator read
// who is ahead by looking, instead of by comparing two numbers. The x clears the longest
// caption either row uses ("ENEMY"/"RIVAL").
//
// The gutter between caption and gauge is a full glyph cell wide because the initiative
// tick lives in it: at the 6px a label needs on its own the mark touched the caption on
// one side and the gauge's first cell on the other, and read as part of the gauge rather
// than as a mark about it. The width is a round 10 cells of 10, so no column is left
// ragged at the end, and it stops where a four-digit numeric still clears the pip.
constexpr int kGaugeX = kMargin + 5 * kFontAdvance + 12;
constexpr int kGaugeW = 100;

// The initiative tick: a bar in that gutter, on the side that acts NEXT. Turns resolve
// on their own and the two fighters do not simply alternate — Speed decides, and a fast
// pet takes two in a row — so "whose turn is it" was a fact the screen had and never
// said, on a screen whose one decision is whether to spend the Exploit before the next
// blow lands.
//
// ACCENT because this IS the token's own meaning — which one is up — and because it is
// free again now that the rival's Health gauge no longer wears it. Presence and position
// carry it in grayscale; the colour only repeats them.
constexpr int kTurnTickW = 3, kTurnTickX = kGaugeX - 8;
void drawTurnTick(Framebuffer& fb, int y, int h) {
    fb.fillRect(kTurnTickX, y, kTurnTickW, h, palColor(Pal::ACCENT));
}
// The outer inset a fighter is seated against before it is allowed to crop, and the
// bounds on the clash lane: never tighter than the strike mark drawn in it, never so
// wide that two small creatures read as ignoring each other.
//
// The bounds are LOGICAL and scale with the shot, because "so wide they read as ignoring
// each other" is a statement about the gap RELATIVE TO the bodies either side of it —
// hold the active-px maximum fixed and the wide shot stands two small creatures the
// better part of a body apart.
constexpr int kStageEdge = 2, kLaneMinLogical = 15, kLaneMaxLogical = 33;

// The floor under all of it: the strike mark is drawn at its authored size whatever the
// shot is, so a lane narrower than the mark has nowhere to travel and the blow stops
// saying which way it is going. UI art is cut at active resolution and shrinking it
// would resample the one thing on the stage that is a pure silhouette, so the LANE
// yields to the mark rather than the other way round.
inline int laneFloor() { return ASSET_UI_STRIKE_COMMON.frameW; }

// The two shots the stage can be framed at, as blitter num/den. Why there are exactly
// two, and why neither resamples, is on CombatStage (combat_screen.h).
constexpr int kShotNum = kScaleNum, kShotDen = kScaleDen;   // the standing shot, x1.75
constexpr int kWideNum = 1, kWideDen = 1;                   // the wide shot, 1:1

// Where a sheet column lands in active px under the creature upscale, as the BLITTER
// puts it: the first destination column whose source sample has reached `x`. Seating
// works in these, not in a plain x*num/den, because two independently truncated
// endpoints can bracket a band one pixel wider than the band itself — and one stray
// column is enough to put a creature inside the lane that exists to keep it out.
constexpr int scaleUp(int x, int num, int den) {
    return (x * num + den - 1) / den;
}

// Seat one fighter's drawing so its band starts at `contentX`, bottom-anchored on the
// shelf. The FRAME is placed from there — a sprite padded inside its cell hangs its
// padding outside the band, which is the whole point of seating by content.
//
// `faceRight` is the direction this SEAT wants its occupant to look, and the sheet's own
// declared Facing (core/render/sprite.h) decides whether that costs a mirror. A sheet
// with no declared facing — the three-quarter standing pose most of the roster is drawn
// in — is never turned, so this is a no-op for it. Seating reads the MIRRORED content
// band for the same reason it reads the band at all: a creature padded to one side of
// its cell would otherwise step that padding's width off its seat the moment it turns.
void drawFighter(Framebuffer& fb, const SpriteData* s, int contentX, int animBeat,
                 uint8_t flashAmt, Rgb565 flashColor, int xNudge, bool faceRight,
                 const AnimClip* pose, int num, int den, const CamoRamp* camo = nullptr,
                 uint8_t camoAmt = 0, const CamoRamp* camoFrom = nullptr) {
    if (!s) return;
    const bool mirror = spriteMirrorToFace(*s, faceRight);
    const int x = contentX - scaleUp(spriteContentX0(*s, mirror), num, den) + xNudge;
    const int y = kSpriteShelf - s->h * num / den;
    // An authored pose plays its own sheet row in order; without one the breathe
    // heuristic runs on row 0, which is the whole of a single-row sheet.
    const int row = pose ? pose->row : 0;
    const int frame = pose ? pose->frameAt(animBeat) : idleFrame(*s, animBeat);
    // The camouflage is what colour the creature IS while it wears it, so the flash goes
    // OVER it rather than instead of it: a fighter struck mid-disguise flashes in the
    // borrowed colours, and a hit stays legible on a disguised fighter without reading as
    // stripping the disguise off. `camoFrom` carries the palette a swap is leaving, so one
    // borrowed set dissolves into the next instead of detouring through the pet's own.
    if (camo && camoAmt > 0 && !camo->empty())
        drawSpriteCamo(fb, *s, frame, x, y, num, den, *camo, camoAmt,
                       flashColor, flashAmt, row, camoFrom, mirror);
    else if (flashAmt > 0)
        drawSpriteFlash(fb, *s, frame, x, y, num, den, flashColor,
                        flashAmt, row, mirror);
    else
        drawSpriteUpscaled(fb, *s, frame, x, y, num, den, row, mirror);
}

// The seat a fighter with no sprite holds: one standard pet cell (the 56x48 logical
// creature cell), so the stage keeps its shape rather than collapsing around whichever
// side has art.
int seatWidth(const SpriteData* s, int num, int den) {
    constexpr int kPetCellW = 56;
    if (!s) return scaleUp(kPetCellW, num, den);
    return scaleUp(spriteContentX1(*s), num, den) -
           scaleUp(spriteContentX0(*s), num, den);
}

// Seat both fighters and the lane at one shot's scale. Split out from the shot PICKER
// (combatStage) so the same seating runs whichever rung is chosen, and so the picker can
// ask "does this one fit" by seating it rather than from a second copy of the arithmetic
// that could drift from it.
CombatStage seatStage(const SpriteData* localSprite, const SpriteData* rivalSprite,
                      int num, int den) {
    const int wL = seatWidth(localSprite, num, den);
    const int wR = seatWidth(rivalSprite, num, den);
    // The lane takes whatever the two fighters leave, held between its bounds.
    const int laneMin = std::max(scaleUp(kLaneMinLogical, num, den), laneFloor());
    const int laneMax = std::max(laneMin, scaleUp(kLaneMaxLogical, num, den));
    int lane = kActiveW - 2 * kStageEdge - wL - wR;
    lane = std::max(laneMin, std::min(laneMax, lane));

    // How much of each fighter stays on canvas. A pair that fits keeps all of both. A
    // pair that doesn't shares the room by HALVES, not evenly: a fighter narrower than
    // its half is never cropped, and the slack it doesn't want goes to the one that
    // does. Splitting the deficit evenly instead would shave pixels off a Process-stage
    // creature for the crime of being matched against a Daemon — the loss belongs to
    // whichever fighter is over its share, and only to it.
    const int avail = kActiveW - 2 * kStageEdge - lane;
    int aL = wL, aR = wR;
    if (wL + wR > avail) {
        const int half = avail / 2;
        if (wL <= half)      { aR = avail - wL; }
        else if (wR <= half) { aL = avail - wR; }
        else                 { aL = half; aR = avail - half; }
    }
    // The VISIBLE group is what gets centred, so a lopsided pair still sits square on
    // the stage. Each fighter then keeps its whole band and runs its surplus off its own
    // outer edge, where the framebuffer drops it.
    const int x0 = (kActiveW - (aL + lane + aR)) / 2;
    CombatStage st;
    st.localW = wL;             st.localX = x0 + aL - wL;
    st.laneX = x0 + aL;         st.laneW = lane;
    st.rivalX = st.laneX + lane; st.rivalW = wR;
    st.num = num;               st.den = den;
    return st;
}

// Does this shot hold BOTH fighters whole? Asked of the seating itself rather than of a
// width sum, so "fits" means exactly what the draw will do — including the lane's own
// clamp, which is what decides how much room the two of them are actually sharing.
bool stageFits(const SpriteData* localSprite, const SpriteData* rivalSprite, int num,
               int den) {
    const CombatStage st = seatStage(localSprite, rivalSprite, num, den);
    return st.localX >= 0 && st.rivalX + st.rivalW <= kActiveW;
}

// The move a combatant actually cast last, following a wildcard slot through to what it
// ROLLED — a metamorphic row is a pool, so the slot's own MoveDef says nothing about
// which line the cast came from and `lastRolled` is the only thing that does.
const MoveDef* castMove(const Combatant& c) {
    if (c.lastMoveIdx < 0) return nullptr;
    if (c.lastMoveIdx < static_cast<int>(c.wildPools.size()))
        return c.wildPools[c.lastMoveIdx].lastRolled;
    return nullptr;
}

// The row this fighter actually cast, whichever kind of slot it came out of: a wildcard
// answers with what it ROLLED, every other slot answers with itself.
//
// Deliberately NOT castMove above, which stays wildcard-only. That one feeds FX_CAMO,
// which asks whether a cast was BORROWED — widen it and an ordinary fighter casting a
// move its opponent happens to carry would start wearing that opponent's colours.
const MoveDef* castRow(const Combatant& c) {
    if (c.lastMoveIdx < 0) return nullptr;
    if (c.lastMoveIdx < static_cast<int>(c.wildPools.size()) &&
        c.wildPools[c.lastMoveIdx].lastRolled)
        return c.wildPools[c.lastMoveIdx].lastRolled;
    if (c.lastMoveIdx < static_cast<int>(c.moves.size())) return c.moves[c.lastMoveIdx];
    return nullptr;
}

// The line a combatant belongs to, or null for one built from a sprite-named spec rather
// than from a creature — a malbeast, a boss, the dummy. That null is the case the kit
// match below exists for.
const char* fighterLine(const Combatant& c) {
    return c.creature ? c.creature->line : nullptr;
}

// Is `m` a row this fighter is carrying? By ID rather than by pointer: both sides resolve
// their rows out of the same registry today, but a combatant built from a spec rather than
// from a loadout is free not to, and colours that only appear on one build path are worse
// than none.
bool kitHolds(const Combatant& c, const MoveDef* m) {
    for (const MoveDef* r : c.moves)
        if (r && r->id && m->id && std::strcmp(r->id, m->id) == 0) return true;
    return false;
}

}  // namespace

// What this ranks and why it ranks that way are on the declaration (combat_screen.h).
// Game::tick is what asks it, once per anim tick.
CamoTarget camoTarget(const Combatant& c, const Combatant& rival) {
    const MoveDef* m = castMove(c);
    if (!m) return {};
    const char* rivalLine = fighterLine(rival);
    const bool theirs =
        kitHolds(rival, m) ||
        (rivalLine && m->line && std::strcmp(m->line, rivalLine) == 0);
    if (theirs) return {CamoTarget::Source::Rival, nullptr};
    const char* own = fighterLine(c);
    if (m->line && own && std::strcmp(m->line, own) != 0)
        return {CamoTarget::Source::Line, m->line};
    return {};
}

// What this builds and why it is built rather than chosen are on the declaration
// (combat_screen.h).
void overridePickerHeader(char* out, size_t cap, bool items, bool lock, bool crew,
                          int roomPx) {
    char bands[32];
    int n = std::snprintf(bands, sizeof(bands), "MOVE");
    auto add = [&](const char* b) {
        n += std::snprintf(bands + n, sizeof(bands) - n, "/%s", b);
    };
    if (items) add("ITEM");
    if (lock) add("LOCK");
    if (crew) add("CREW");
    char titled[48];
    std::snprintf(titled, sizeof(titled), "EXPLOIT: %s", bands);
    std::snprintf(out, cap, "%s", textWidth(titled) <= roomPx ? titled : bands);
}

// What this answers and why it is answered off the MOVE are on the declaration
// (combat_screen.h).
StrikeMark strikeMark(const Combatant& actor, int strikeSeq) {
    const MoveDef* m = castRow(actor);
    const SpriteData* sheet = &ASSET_UI_STRIKE_COMMON;
    if (m && m->line) {
        if (std::strcmp(m->line, "ransomware") == 0) sheet = &ASSET_UI_STRIKE_RANSOMWARE;
        else if (std::strcmp(m->line, "phishing") == 0) sheet = &ASSET_UI_STRIKE_PHISHING;
        else if (std::strcmp(m->line, "trojan") == 0) sheet = &ASSET_UI_STRIKE_TROJAN;
        else if (std::strcmp(m->line, "worm") == 0) sheet = &ASSET_UI_STRIKE_WORM;
    }
    const int n = sheet->frames > 0 ? sheet->frames : 1;
    // Modulo the fight's swing count, so the pair walks however many frames the sheet
    // actually ships — a source drawn with one is simply never alternated.
    return {sheet, ((strikeSeq % n) + n) % n};
}

// The threshold and why it is measured this way are on the declaration
// (combat_screen.h).
bool hurtPoseEarned(const Combatant& target, int damage) {
    if (target.lockedTurnsLeft > 0) return true;
    return target.maxHealth > 0 &&
           damage * 100 >= kHurtPosePctOfMax * target.maxHealth;
}

// Pose precedence and its reasoning are on the declaration (combat_screen.h).
const AnimClip* fightPose(const Combatant& c, bool takingHit, bool swinging) {
    if (!c.creature) return nullptr;
    if (takingHit)
        if (const AnimClip* hurt = c.creature->clip("hurt")) return hurt;
    if (swinging)
        if (const AnimClip* attack = c.creature->clip("attack")) return attack;
    return c.creature->clip("idle");
}

// The seating rule and why it is this one are on the declaration (combat_screen.h).
CombatStage combatStage(const SpriteData* localSprite, const SpriteData* rivalSprite) {
    // Frame the pair: the standing shot if both fit in it whole, the wide shot if not.
    // Asked in that order and stopping at the first that fits, so a fight is only ever
    // pulled back as far as it has to be. See CombatStage (combat_screen.h) for why the
    // ladder has these two rungs and nothing between them.
    int num = kShotNum, den = kShotDen;
    if (!stageFits(localSprite, rivalSprite, num, den) &&
        stageFits(localSprite, rivalSprite, kWideNum, kWideDen)) {
        num = kWideNum;
        den = kWideDen;
    }
    return seatStage(localSprite, rivalSprite, num, den);
}

namespace {

// Wind-up cue: a channelling combatant's silhouette CHARGES toward warn-bright over
// kWindupFlashPeriod anim-ticks and drops back, repeating for as long as the wind-up
// lasts.
//
// The ramp climbs rather than decaying, and that direction is the whole cue. An impact
// (impactFlashAmt below) snaps to its peak and fades, because that is what being hit
// looks like; a wind-up that did the same read as a hit landing on the charging fighter,
// or as a buff it had just cast on itself. Building light is accumulation, and pairs
// with drawWindupMark's countdown over the same fighter — the countable half, and the
// one that survives grayscale.
constexpr int kWindupFlashPeriod = 8;
uint8_t windupFlashAmt(bool channeling, int animBeat) {
    if (!channeling) return 0;
    const int t = animBeat % kWindupFlashPeriod;
    return static_cast<uint8_t>(180 * (t + 1) / kWindupFlashPeriod);
}

// The wind-up's countable half: a segment meter riding directly over the head of the
// fighter that is charging, with a caret under it pointing at its owner. One cell per
// turn of the move's whole wind-up (MoveDef::channelTurns), lit down to the turns still
// to run — so the meter says how long the charge IS as well as how much is left, and a
// single-turn remainder still reads as a countdown rather than as a lone blob.
//
// Both fighters get one, from the same code: a cue that only ever appeared over the
// opponent would teach the operator nothing about its own pet's charge, which is the
// half it CHOSE. Over the head rather than at the feet because the shelf there is
// already spoken for (a Worm's replica board stands on it), and because a fighter's
// outer flank moves off-canvas for an oversized cell that crops.
constexpr int kWindupSeg = 7, kWindupSegH = 8, kWindupCaret = 4;
void drawWindupMark(Framebuffer& fb, int midX, int headY, int turnsLeft, int turnsTotal) {
    const int n = std::max(turnsLeft, std::max(1, turnsTotal));
    const int w = n * kWindupSeg + 2;
    // Held inside the canvas and under the chrome: a cropping Daemon's midpoint can sit
    // past either screen edge, and a tall one's head can rise past the header — a marker
    // that says "this one" has to be on screen, and clear of the rows above, to say it.
    const int x = std::max(kMargin, std::min(kActiveW - kMargin - w, midX - w / 2));
    const int y = std::max(kHeaderRule + 22, headY - kWindupSegH - kWindupCaret - 2);
    fb.fillRect(x, y, w, kWindupSegH, palColor(Pal::INK_DIM));
    for (int i = 0; i < n; ++i)
        fb.fillRect(x + 1 + i * kWindupSeg, y + 1, kWindupSeg - 1, kWindupSegH - 2,
                    palColor(i < turnsLeft ? Pal::WARN : Pal::TRACK));
    for (int i = 0; i < kWindupCaret; ++i)
        fb.fillRect(x + w / 2 - kWindupCaret + i, y + kWindupSegH + i,
                    2 * (kWindupCaret - i) - 1, 1, palColor(Pal::WARN));
}

// The strike mark: WHO is hitting WHOM, WITH WHAT, drawn in the clash lane for the
// swing window.
//
// It TRAVELS in the direction of the blow, crossing the lane from the attacker's edge to
// its target's over the window and fading as it goes. Motion carries the direction, and
// the sheet is drawn as though the blow travels right and mirrored for one going left
// (SpriteData::facing), so a frozen frame answers "which way" as well as play does.
//
// WHAT is the source of the cast — its line, or the common pool — and each source owns a
// PAIR, walked on the fight's own swing count so no two blows in a row draw the same
// frame (strikeMark, combat_screen.h). One mark per source read as wallpaper within a few
// turns; two make each swing its own event. The art and the reasoning behind each shape
// are tools/gen_fight_art.py.
//
// It cuts across torso height — high enough to cross a short creature's body rather than
// its feet, low enough to stay under a tall one's head. Stated in LOGICAL rows off the
// shelf and scaled by the shot, so it keeps crossing the same part of the body on a
// wide-shot stage instead of sailing over the heads of two creatures that just got
// shorter.
constexpr int kStrikeLogicalY = 26;
constexpr int strikeY(int num, int den) {
    return kSpriteShelf - kStrikeLogicalY * num / den;
}

void drawStrikeMark(Framebuffer& fb, int laneX, int laneW, int dir, int beat, int period,
                    const SpriteData& mark, int variant, int num, int den) {
    if (beat < 0 || beat >= period) return;
    const int travel = std::max(0, laneW - mark.frameW);
    const int from = dir > 0 ? laneX : laneX + travel;
    const int x = from + dir * travel * beat / std::max(1, period - 1);
    const uint8_t a = static_cast<uint8_t>(255 * (period - beat) / period);
    drawSpriteTinted(fb, mark, variant % std::max(1, mark.frames), x,
                     strikeY(num, den) - mark.h / 2, palColor(Pal::INK), /*row=*/0,
                     spriteMirrorToFace(mark, /*faceRight=*/dir > 0), a);
}

// Impact "punch" cue (no new art/frames): the side that just took a landed hit
// recoils a few active-px AWAY from its opponent and flashes white, decaying over
// kImpactPeriod anim-ticks — an "a hit just happened" tell that reads independently
// of the breathe-loop frame, so an accelerated feeding-frenzy streak still feels like
// a string of impacts rather than a silent, motionless damage number.
constexpr int kImpactPeriod = 4;


int impactNudgePx(int hitBeat, int dir) {   // dir: -1 (shove left) / +1 (shove right)
    if (hitBeat < 0 || hitBeat >= kImpactPeriod) return 0;
    return dir * (kImpactPeriod - hitBeat) * 2;   // 8 -> 6 -> 4 -> 2 -> 0 active-px
}
uint8_t impactFlashAmt(int hitBeat) {
    if (hitBeat < 0 || hitBeat >= kImpactPeriod) return 0;
    return static_cast<uint8_t>(200 * (kImpactPeriod - hitBeat) / kImpactPeriod);
}

// The damage popup's own window, in anim-ticks: one ordinary turn
// (kCombatBeatsPerTurn heartbeats at kCombatAnimMs each), so the number is still up when
// the fight asks what just happened and gone before the next blow lands. A feeding-frenzy
// streak that resolves faster simply cuts it short with the next hit, which is right —
// the newest number is the one being read.
constexpr int kDamagePopPeriod = 6;
constexpr int kDamagePopRise = 10;         // active px it climbs over that window

// How far a popup has floated, or -1 once it is spent.
int damagePopRise(int hitBeat) {
    if (hitBeat < 0 || hitBeat >= kDamagePopPeriod) return -1;
    return kDamagePopRise * hitBeat / (kDamagePopPeriod - 1);
}

// Attack "hop" cue (no new art/frames, no change to either fighter's resting stage
// position): the combatant that just acted steps a couple of active-px TOWARD its
// target and the target steps the same distance AWAY, decaying over
// kAttackHopPeriod anim-ticks. Unlike the impact punch above — which needs a landed
// hit — this fires on every resolved STRIKE (combat.h lastWasStrike), so a fully
// shielded swing still reads as "who just attacked" instead of standing still, while a
// defend, an item or a ransom bill coming due moves nobody.
// Because the local seat sits left of the rival seat, "attacker forward" and
// "target away" happen to point the same screen direction for BOTH fighters on a
// given turn, so one dir/beat pair drives both sprites.
constexpr int kAttackHopPeriod = 4;
int attackHopPx(int hopBeat, int dir) {
    if (hopBeat < 0 || hopBeat >= kAttackHopPeriod) return 0;
    return dir * (kAttackHopPeriod - hopBeat);   // 4 -> 3 -> 2 -> 1 -> 0 active-px
}

// --- Worm replicas ------------------------------------------------------------------
// The replication board, drawn on the same shelf as its parent so a worm and its copies
// read as one force rather than as a pet with UI stuck to it. The glyphs, their frame
// pairs and how one is seated are in core/ui/worm_replicas.h, shared with the idle
// habitat — only the fight's own reading of them is below.

// How long a destroyed copy's dissolve plays, in anim-ticks — matched to the impact
// punch (kImpactPeriod) so the pop and the recoil that caused it read as one beat.
constexpr int kReplicaDeathPeriod = 4;

// One side's replicas, plus the dissolve of a copy this turn destroyed (kill.happened).
//
// The board stands BETWEEN its parent and the enemy — that is what a copy is for, so it
// is where it stands. `frontX` is the parent's seat edge facing the opponent and
// `stride` steps AWAY from that edge (negative for the left seat, positive for the
// right), so slot 0 is always the front rank nearest the enemy and later copies fall in
// behind it. Anchoring at the front rather than distributing across the seat is what
// keeps the line stable: gaining or losing a copy adds or removes one at the BACK
// instead of sliding every copy sideways.
//
// `attacking` plays the chomp pair — true for the side that just acted, so the whole
// board swings together with its parent.
void drawReplicaRow(Framebuffer& fb, const Combatant& c, bool attacking,
                    const WormKill& kill, int killBeat, int frontX, int stride,
                    int shelfY, int animBeat, int num, int den) {
    const bool dying = kill.happened && killBeat >= 0 && killBeat < kReplicaDeathPeriod;
    const int n = c.wormReplicaCount + (dying ? 1 : 0);
    if (n <= 0) return;
    const int base = attacking ? kReplicaAttackFrame : kReplicaIdleFrame;
    // Back to front, so the nearer rank overlaps the one behind it if the two ever meet.
    for (int i = n - 1; i >= 0; --i) {
        const int cx = frontX + stride * i + stride / 2;
        // The dissolve takes the slot BEHIND the live board rather than the slot its
        // copy actually held — which is unknowable, since the array packs the moment one
        // dies. It reads as the board having lost its last rank, which is the true part.
        const bool ghost = dying && i == n - 1;
        const bool defender = ghost ? kill.defender : c.wormReplicas[i].defender;
        const SpriteData& s = defender ? ASSET_SPR_WORM_REPLICA_DEFEND
                                       : ASSET_SPR_WORM_REPLICA_ATTACK;
        const int frame = (ghost ? kReplicaDeathFrame : base) + (animBeat & 1);
        drawReplica(fb, s, frame, cx, shelfY, num, den);
    }
}

// How many rows each panel page will actually draw. Asked BEFORE the box is filled, so
// the floor can be cut to the content instead of to the worst case — see the panel block
// in drawCombat for why the top is the anchored edge and the floor is the one that moves.
//
// These count what the draw below emits, and the two have to stay in step: a page that
// drew a row this did not count would run past its own floor and be clipped by it. They
// are kept immediately beside each other for that reason, and each names the rows it is
// counting in the order the draw emits them.
int vsPageRows(const Combatant& local, const Combatant& rival) {
    const CombatVsGrid g = combatVsGrid(local, rival, /*localGuard=*/true);
    int n = 1 + g.n;                     // the YOU/RIVAL column header, then the grid
    if (local.crewExploit.live() || rival.crewExploit.live()) {
        ++n;                             // the separator costs about a row of height
        if (local.crewExploit.live()) ++n;
        if (rival.crewExploit.live()) ++n;
    }
    return n;
}

int kitPageRows(const Combatant& rival, const RivalPrizes& prizes) {
    int n = 1;                           // the rival's name-and-Health row
    for (const MoveDef* m : rival.moves)
        if (m) ++n;
    if (rival.autoExploit.label && !rival.autoExploitFired) ++n;
    if (prizes.mask) ++n;                // the "+ WIN TO LEARN" legend under the list
    return n;
}

// The passive strip — one combatant's live line-passive state as a bar plus a pip row,
// drawn immediately outside its Health gauge. Both fighters get one: a passive changes who
// wins, so hiding the opponent's would leave the player watching a fight decided by
// something they can't see (in a duel, by the pet they're fighting). The rival's is drawn
// smaller, which is the whole reason this takes its sizes as parameters — same language,
// less weight, because the pet you're steering is the one you act on.
//
// A pet has exactly ONE line, so the states below are mutually exclusive in practice and
// share the strip rather than stacking rows: a Phishing shield, a Worm replication board,
// a Trojan trap stack and a Ransomware pool never co-occur on the same combatant.
void drawPassiveStrip(Framebuffer& fb, const Combatant& c, int x, int y, int w, int barH,
                      int pipW, int pipH, int beat) {
    const int gap = 2;
    auto pips = [&](int lit, int total, Pal on) {
        for (int i = 0; i < total; ++i)
            fb.fillRect(x + w + 6 + i * (pipW + gap), y, pipW, pipH,
                        palColor(i < lit ? on : Pal::INK_DIM));
    };
    // Obfuscation shield (Phishing, combat.h shieldHp): shieldHp can outnumber maxHealth,
    // so the fill isn't a literal ratio — 1-e^-x saturates toward a full bar as the pool
    // grows past current max Health without ever overflowing it. Once it genuinely exceeds
    // max Health the leading edge churns with ink-white flecks, so "there's more here than
    // this bar can draw" reads too.
    if (c.shieldHp > 0) {
        const float ratio = c.maxHealth > 0
            ? static_cast<float>(c.shieldHp) / c.maxHealth : 0.f;
        drawProgressBar(fb, x, y, w, barH, 1.0f - std::exp(-ratio), palColor(Pal::TEAM_BLUE),
                        c.shieldHp > c.maxHealth, beat);
        // Frenzy ribs. The churn above says "this pool is bigger than the bar can draw"
        // — a statement about SIZE, live every frame the pool is over max Health. The
        // frenzy is a different claim (the pet has committed to spending the wall) and
        // it OUTLASTS the churn, holding until the pool pops, so the two cannot share
        // one tell. Ribs rather than a hue shift because this strip is read in grayscale
        // like every other gauge: the count is the signal, and it climbs 1..4 with the
        // lean, so "how committed" reads at a glance without a legend.
        const int leanPct = phishFrenzyLeanPct(c);
        if (leanPct > 0 && barH > 2) {
            const int ribs = 1 + leanPct * 3 / kPhishFrenzyLeanMaxPct;
            for (int i = 0; i < ribs; ++i) {
                const int rx = x + 2 + (w - 4) * (i + 1) / (ribs + 1);
                fb.fillRect(rx, y + 1, 1, barH - 2, palColor(Pal::PAPER));
            }
        }
        return;
    }
    // Worm replication slots (combat.h wormReplicaCount): one pip per slot, up to
    // kWormReplicaSlots — "how much replication is left" is the worm's whole resource,
    // and it is a count with no magnitude, so it reads as pips like the trap stack does.
    // The two kinds are told apart by FILL, not by hue: a defender's pip is solid and an
    // attacker's is a hollow outline, because which sort is standing decides what the
    // other sort is worth (wormReplicaDamage) and that has to survive grayscale.
    if (c.wormReplicaCount > 0) {
        for (int i = 0; i < kWormReplicaSlots; ++i) {
            const int px = x + i * (pipW + gap * 2), pw = pipW + gap;
            if (i >= c.wormReplicaCount) {
                fb.fillRect(px, y, pw, pipH, palColor(Pal::INK_DIM));
            } else if (c.wormReplicas[i].defender) {
                fb.fillRect(px, y, pw, pipH, palColor(Pal::ACCENT));
            } else {
                fb.fillRect(px, y, pw, pipH, palColor(Pal::ACCENT));
                if (pw > 2 && pipH > 2)
                    fb.fillRect(px + 1, y + 1, pw - 2, pipH - 2, palColor(Pal::PAPER));
            }
        }
        return;
    }
    // Trojan traps (combat.h trojanTrapCount): one pip per armed trap, up to the cap —
    // "how many traps are laid", dual-coded by count so it survives grayscale. No bar:
    // a trap stack has a count and no magnitude.
    if (c.trojanTrapCount > 0) {
        for (int i = 0; i < kTrojanTrapCap; ++i)
            fb.fillRect(x + i * (pipW + gap * 2), y, pipW + gap, pipH,
                        palColor(i < c.trojanTrapCount ? Pal::WARN : Pal::INK_DIM));
        return;
    }
    // Ransom pool (Ransomware, combat.h ransomPool): damage the passive is HOLDING. Green
    // because none of it has been taken yet — the Health gauge below is telling the truth
    // while this is up. Same saturating curve as the shield (the pool can outgrow max
    // Health), floored so the first small hit held still draws a visible sliver: "there is
    // a bill" has to read before "how big". The pips count the turns still owed.
    if (c.ransomPool > 0) {
        const float ratio = c.maxHealth > 0
            ? static_cast<float>(c.ransomPool) / c.maxHealth : 0.f;
        drawProgressBar(fb, x, y, w, barH,
                        std::max(0.06f, 1.0f - std::exp(-ratio)), palColor(Pal::CALM));
        pips(c.ransomTurnsLeft, kRansomHoldTurns, Pal::CALM);
    }
}

} // namespace

CombatStatusStrip combatStatusStrip(const Combatant& c, bool withGuard) {
    CombatStatusStrip s;
    auto add = [&](CombatVsKind k, int count) {
        for (int i = 0; i < count && s.n < CombatStatusStrip::kCap; ++i) s.k[s.n++] = k;
    };
    // Ordered by how much it changes the next exchange, because the strip is drawn into
    // a fighter's own width and a wide one runs out of room before a narrow one does.
    if (c.lockedTurnsLeft > 0) add(CombatVsKind::Stun, 1);
    if (c.dotTurnsLeft > 0) add(CombatVsKind::Dot, 1);
    if (c.shieldHp > 0) add(CombatVsKind::Shield, 1);
    if (withGuard && c.guard > 0) add(CombatVsKind::Guard, 1);
    if (c.ransomPool > 0) add(CombatVsKind::Ransom, 1);
    // The two countable ones. A trap pile and a carried drive are small numbers with a
    // hard ceiling, so repeating the glyph says the count without spending a digit.
    add(CombatVsKind::Trap, c.trojanTrapCount);
    if (c.itemShield) add(CombatVsKind::Backup, 1);
    return s;
}

// Which frame of a status glyph is up. A sheet with one cell holds still and one with
// more walks them, so giving a condition an animation is an art change and not a code
// change — the same bargain idleFrame makes for creatures (core/render/sprite.h). Halved
// off the anim beat because the strip sits under a fighter's feet: at the full rate a
// rocking skull competes with the fight it is describing.
int glyphFrame(const SpriteData& s, int animBeat) {
    return s.frames > 1 ? (animBeat / 2) % s.frames : 0;
}

const SpriteData* combatVsGlyph(CombatVsKind kind) {
    switch (kind) {
        case CombatVsKind::Health:  return &ASSET_ICON_FIGHT_HP;
        case CombatVsKind::Power:   return &ASSET_ICON_FIGHT_PWR;
        case CombatVsKind::Defense: return &ASSET_ICON_FIGHT_DEF;
        case CombatVsKind::Speed:   return &ASSET_ICON_FIGHT_SPD;
        case CombatVsKind::Stun:    return &ASSET_ICON_FIGHT_STUN;
        case CombatVsKind::Dot:     return &ASSET_ICON_FIGHT_DOT;
        case CombatVsKind::Shield:  return &ASSET_ICON_FIGHT_SHLD;
        case CombatVsKind::Guard:   return &ASSET_ICON_FIGHT_GRD;
        case CombatVsKind::Ransom:  return &ASSET_ICON_FIGHT_RNSM;
        case CombatVsKind::Backup:  return &ASSET_ICON_FIGHT_BKUP;
        case CombatVsKind::Trap:    return &ASSET_ICON_FIGHT_TRAP;
        case CombatVsKind::Copy:    return &ASSET_ICON_FIGHT_COPY;
    }
    return nullptr;
}

CombatVitals combatVitals(const Combatant& c) {
    CombatVitals v;
    v.health = c.health > 0 ? c.health : 0;
    v.maxHealth = c.maxHealth;
    // Attack power is the EFFECTIVE figure. Two different mechanics push on the same
    // number from different fields — a siphon shifts powerMultPct while a Lockout stack
    // accumulates in stackPowerBonus, and combat multiplies by the SUM (combat.cpp's
    // applyEffect). Reading only one of them makes a stacked pet look like it is purely
    // losing ground to a drain it is in fact out-earning.
    v.power = c.powerMultPct + c.stackPowerBonus;
    // The incoming-damage cut under the never-immune clamp the attack path applies, plus
    // the Cipher track feeding it.
    v.defense = c.dmgReducePct + c.stackDefenseBonus;
    if (v.defense > kLevelDmgReduceMaxPct) v.defense = kLevelDmgReduceMaxPct;
    // Speed is float (a Phishing siphon steals fractional amounts); round to the nearest
    // whole tick — a decimal point would break the tabular-digit convention every other
    // numeric on the device follows.
    v.speed = static_cast<int>(std::lround(c.speed));
    return v;
}

void CombatVsGrid::push(CombatVsKind kind, const char* tag, const char* a,
                        const char* b) {
    if (n >= kCap) return;
    r[n].kind = kind;
    std::snprintf(r[n].tag, sizeof(r[n].tag), "%s", tag);
    std::snprintf(r[n].local, sizeof(r[n].local), "%s", a ? a : "");
    std::snprintf(r[n].rival, sizeof(r[n].rival), "%s", b ? b : "");
    ++n;
}

bool CombatVsGrid::has(const char* tag) const {
    for (int i = 0; i < n; ++i)
        if (std::strcmp(r[i].tag, tag) == 0) return true;
    return false;
}

CombatVsGrid combatVsGrid(const Combatant& local, const Combatant& rival,
                          bool localGuard) {
    CombatVsGrid g;
    char a[12], b[12];
    // A pair of cells built the same way for both fighters — the whole grid is this
    // shape, and writing it once is what stops one side from being formatted (or
    // forgotten) differently from the other.
    auto pair = [&](CombatVsKind kind, const char* tag, bool live) {
        if (live) g.push(kind, tag, a, b);
    };
    auto num = [](char* out, size_t cap, int v, bool live) {
        if (live) std::snprintf(out, cap, "%d", v);
        else out[0] = '\0';
    };

    const CombatVitals lv = combatVitals(local), rv = combatVitals(rival);

    // --- The four vitals, always. These are the fight. ------------------------------
    std::snprintf(a, sizeof(a), "%d/%d", lv.health, lv.maxHealth);
    std::snprintf(b, sizeof(b), "%d/%d", rv.health, rv.maxHealth);
    pair(CombatVsKind::Health, "HP", true);
    // Power's delta is everything pushing on it at once — a Phishing siphon moving
    // powerMultPct and a Ransomware Lockout stack accumulating in stackPowerBonus, which
    // combat multiplies by the SUM. One signed figure says which way the pet is going;
    // the effective number beside it says where it has got to.
    auto pwrCell = [](char* out, size_t cap, const Combatant& c, const CombatVitals& v) {
        const int d = (c.powerMultPct - c.basePowerMultPct) + c.stackPowerBonus;
        if (d) std::snprintf(out, cap, "%d%+d", v.power, d);
        else std::snprintf(out, cap, "%d", v.power);
    };
    pwrCell(a, sizeof(a), local, lv);
    pwrCell(b, sizeof(b), rival, rv);
    pair(CombatVsKind::Power, "PWR", true);
    auto defCell = [](char* out, size_t cap, const Combatant& c, const CombatVitals& v) {
        if (c.stackDefenseBonus) std::snprintf(out, cap, "%d%+d", v.defense,
                                               c.stackDefenseBonus);
        else std::snprintf(out, cap, "%d", v.defense);
    };
    defCell(a, sizeof(a), local, lv);
    defCell(b, sizeof(b), rival, rv);
    pair(CombatVsKind::Defense, "DEF", true);
    auto spdCell = [](char* out, size_t cap, const Combatant& c, const CombatVitals& v) {
        const int d = static_cast<int>(std::lround(c.speed - c.baseSpeed));
        if (d) std::snprintf(out, cap, "%d%+d", v.speed, d);
        else std::snprintf(out, cap, "%d", v.speed);
    };
    spdCell(a, sizeof(a), local, lv);
    spdCell(b, sizeof(b), rival, rv);
    pair(CombatVsKind::Speed, "SPD", true);

    // --- What changes what you would do NEXT turn ------------------------------------
    // Frozen: how many turns are left in the lock. Free but still carrying resistance from
    // one: the odds the NEXT stun sticks (stunLandPct). Which of the two a fighter needs is
    // exactly which one it is showing — the number flips the moment the lock lifts, and a
    // fighter that has shaken the whole pile off drops out of the row.
    auto stunCell = [](char* out, size_t cap, const Combatant& c) {
        if (c.lockedTurnsLeft > 0) std::snprintf(out, cap, "%d", c.lockedTurnsLeft);
        else if (c.lockResist > 0) std::snprintf(out, cap, "%d%%", stunLandPct(c));
        else out[0] = '\0';
    };
    stunCell(a, sizeof(a), local);
    stunCell(b, sizeof(b), rival);
    pair(CombatVsKind::Stun, "STUN", a[0] || b[0]);

    auto dotCell = [](char* out, size_t cap, const Combatant& c) {
        if (c.dotTurnsLeft > 0) std::snprintf(out, cap, "%dx%d", c.dotPerTurn,
                                              c.dotTurnsLeft);
        else out[0] = '\0';
    };
    dotCell(a, sizeof(a), local);
    dotCell(b, sizeof(b), rival);
    pair(CombatVsKind::Dot, "DOT", local.dotTurnsLeft > 0 || rival.dotTurnsLeft > 0);

    num(a, sizeof(a), local.shieldHp, local.shieldHp > 0);
    num(b, sizeof(b), rival.shieldHp, rival.shieldHp > 0);
    pair(CombatVsKind::Shield, "SHLD", local.shieldHp > 0 || rival.shieldHp > 0);

    // The brace is the local pet's alone: a rival's is spent before this could be read.
    num(a, sizeof(a), local.guard, localGuard && local.guard > 0);
    b[0] = '\0';
    pair(CombatVsKind::Guard, "GRD", localGuard && local.guard > 0);

    // The ransom pool and the turns before it lands — the one affliction whose VALUE is
    // the decision, since it is how much is about to arrive and when.
    auto rnsmCell = [](char* out, size_t cap, const Combatant& c) {
        if (c.ransomPool > 0) std::snprintf(out, cap, "%d/%d", c.ransomPool,
                                            c.ransomTurnsLeft);
        else out[0] = '\0';
    };
    rnsmCell(a, sizeof(a), local);
    rnsmCell(b, sizeof(b), rival);
    pair(CombatVsKind::Ransom, "RNSM", local.ransomPool > 0 || rival.ransomPool > 0);

    // A carried drive, a trap pile and the worm copies are NOT rows here. Each is a
    // presence or a small count, which the fighter's own status strip says by showing
    // (and repeating) its glyph, and the copies are bodies on the shelf already. A row
    // would be a second, worse copy of something the screen is not hiding.
    return g;
}

void drawCombat(Framebuffer& fb, const Combat& combat,
                const SpriteData* playerSprite, const SpriteData* enemySprite,
                int beat, int animBeat, int hitBeat, int statPage,
                const CombatSides& sides, const CombatOutro& outro,
                const RivalPrizes& prizes, const CombatCamo& camo) {
    fb.clear(palColor(Pal::PAPER));
    // LOCAL and RIVAL are ROLES, not Combat's player/enemy slots. Everything the screen
    // says and seats is bound to the role: the local pet gets the bottom, zoned Health
    // gauge with its Critical pulse (the "my pet is in trouble" read) and the left-hand
    // stage seat, in every fight. In a duel the guest holds Combat's enemy_ slot
    // (core/model/pvp_battle.h), so the two are swapped for DISPLAY only. Resolution is
    // untouched: mirroring the fight itself would desync the two devices, which is
    // exactly why this is a render-side rebinding.
    const bool flip = sides.localIsEnemySide;
    const Combatant& en = flip ? combat.player() : combat.enemy();   // the rival
    const Combatant& pl = flip ? combat.enemy() : combat.player();   // the local pet
    const SpriteData* rivalSprite = flip ? playerSprite : enemySprite;
    const SpriteData* localSprite = flip ? enemySprite : playerSprite;
    // "did the LOCAL combatant land the last hit" — the local frame of reference for
    // the damage popup and the impact nudge, both of which follow the target.
    const bool lastByLocal = combat.lastByPlayer() != flip;
    // ...and who acts next, rebound the same way. Only meaningful while the fight is
    // running: a settled fight has no next turn, and a tick still pointing at somebody
    // would read as one more exchange coming.
    const bool ongoing = combat.outcome() == Combat::Outcome::Ongoing;
    const bool localTurnNext = combat.playerTurnNext() != flip;

    // --- Enemy header: name + level + override pip ----------------------------
    // Difficulty pips already showed once, on the pre-fight encounter intro
    // (drawEncounterIntro, expl_screen.cpp) — repeating them here just ate a
    // full header row for no new information, so this HUD skips them.
    //
    // Override pip (UI_OVERRIDE_PIP): bright ready / greyed spent. With more
    // than one Exploit use a small "xN" count of the remaining uses rides
    // alongside so the extra allowance reads without colour.
    const SpriteData& pip = combat.overrideReady() ? ASSET_ICON_OVERRIDE_PIP
                                                    : ASSET_ICON_OVERRIDE_PIP_SPENT;
    const int pipX = kActiveW - kMargin - pip.frameW;
    drawSprite(fb, pip, 0, pipX, 6);
    int hintX = pipX;
    if (combat.overrideUsesTotal() > 1) {
        char cnt[8];
        std::snprintf(cnt, sizeof(cnt), "x%d", combat.overrideUsesLeft());
        hintX -= 3 + textWidth(cnt);
        drawText(fb, hintX, 6, cnt, palColor(Pal::INK));
    }
    const int chordX = hintX - 4 - textWidth("A+C");
    drawText(fb, chordX, 6, "A+C", palColor(Pal::INK_DIM));

    // Level readout: "???" for Sim dummies/bosses (never level-tagged) so the gap reads
    // as "unranked", never as a misleading "Lv 0".
    //
    // It rides the NAME's row because that is the row it belongs to — what a rival IS,
    // as against what it currently has left — and because the row below it is now two
    // gauges wide. The pip hangs a full 16px cell into that lower row, so a readout
    // parked at the end of it was drawn straight through by the pip's disc.
    char lvBuf[8];
    if (en.hasLevel) std::snprintf(lvBuf, sizeof(lvBuf), "Lv %d", en.level);
    else std::snprintf(lvBuf, sizeof(lvBuf), "Lv ???");
    // A full cell of air off the chord hint: both are dim, so a tighter gap ran the two
    // together into one unreadable "LV ??? A+C".
    const int lvX = chordX - kFontAdvance - textWidth(lvBuf);
    drawText(fb, lvX, kTitleY, lvBuf, palColor(Pal::INK_DIM));
    // The name takes what the level leaves, and travels when it cannot fit — a boss's
    // name is the longest thing this row ever carries and the level behind it is fixed.
    drawTextMarquee(fb, kMargin, kTitleY, lvX - 6 - kMargin, en.name,
                    palColor(Pal::INK), beat, /*scroll=*/true);

    // --- Enemy Health (neutral — emptying is good) + channel wind-up ----------
    // This row reuses the y-band the encounter-intro header gives its difficulty pips.
    //
    // THE SAME WIDGET AS THE PET'S OWN, at the same x and the same width, so the two
    // read as one comparison rather than as two unrelated instruments. They used to be
    // a solid progress bar up here against a segmented gauge down there, at different
    // widths and different origins — the same quantity in two grammars, which asks a
    // reader to learn both and still leaves them unable to see at a glance who is
    // ahead. Position says whose a gauge is; the shared shape is what makes them
    // comparable.
    //
    // What stays different is the one thing that IS different: the pet's carries the
    // danger ramp and its Critical pulse, and the rival's is drawn in a neutral ink,
    // because the same amount of rival Health is good news or bad depending on which
    // side you read it from. That also gets ACCENT off a status quantity, where
    // VISUAL_LANGUAGE 1.3 does not allow it — the picker's cursor is ACCENT, and on a
    // frame with the picker open the rival's Health bar was wearing the selection
    // colour.
    drawText(fb, kMargin, 19, sides.rivalLabel, palColor(Pal::INK_DIM));
    const Rgb565 neutral = palColor(Pal::INK);
    const int ehPct = en.maxHealth > 0 ? en.health * 100 / en.maxHealth : 0;
    drawGauge(fb, kGaugeX, 18, kGaugeW, 10, ehPct, Zone::Ok, false, false, beat,
              &neutral);
    if (ongoing && !localTurnNext) drawTurnTick(fb, 18, 10);
    // Its numeric, on the same right edge as the pet's. A bar alone cannot tell 3 left
    // from 30, and the rival's exact Health is not a secret the screen was keeping — the
    // panel's VS and KIT pages both print it, one keypress away.
    char ehp[16];
    std::snprintf(ehp, sizeof(ehp), "%d", en.health > 0 ? en.health : 0);
    drawText(fb, kGaugeX + kGaugeW + 6, 19, ehp, palColor(Pal::INK));
    const int ehX = kGaugeX, ehW = kGaugeW;   // the passive strip rides the same band
    // The rival's passive strip, shrunk, tucked under its Health bar — the same visual
    // language as the pet's own, at less weight. Its line passive decides the fight just
    // as much as yours does, and until now it only ever showed on the device of whoever
    // owned it, which in a duel is the opponent's screen.
    drawPassiveStrip(fb, en, ehX, 30, ehW, 3, /*pipW=*/4, /*pipH=*/3, beat);
    // UI_MOVE_CHANNEL cue — read off the RIVAL rather than Combat's enemy accessor, so
    // the wind-up warning still describes the pet you're looking at when the roles are
    // swapped.
    const MoveDef* topChannel =
        (en.channelMoveIdx >= 0 && en.channelMoveIdx < static_cast<int>(en.moves.size()))
            ? en.moves[en.channelMoveIdx] : nullptr;
    if (topChannel) {
        char buf[28];
        std::snprintf(buf, sizeof(buf), "WINDUP %s %d", topChannel->displayName,
                      en.channelLeft);
        drawText(fb, kMargin, 33, buf, palColor(Pal::WARN));
    }

    // --- Both combatant sprites (reused SPR_PET_* idle frames) --------------
    // Stage seating: the LOCAL pet holds the LEFT seat and its rival the RIGHT one, so a
    // fight reads left-to-right as "mine, then theirs". A seat also states which way its
    // occupant LOOKS — left seat rightward, right seat leftward — and a sheet drawn the
    // other way round is mirrored into it (drawFighter), so neither fighter is ever shown
    // the back of the thing it is fighting. The seat follows the local/rival ROLE (`flip`),
    // never Combat's player_/enemy_ slot, so a duel guest sees its own pet on the left
    // exactly like the host does. Bottom-anchored on the shelf; a sprite taller than the
    // band extends upward rather than clipping. Both bands and the lane between them come
    // from combatStage() — see combat_screen.h for why the lane is reserved first.
    const CombatStage stage = combatStage(localSprite, rivalSprite);
    // The shot this pairing earned. Everything seated on the stage below reads it rather
    // than kScaleNum/kScaleDen, so the wide shot pulls the whole picture back together
    // instead of shrinking the creatures inside chrome that stayed where it was.
    const int shotN = stage.num, shotD = stage.den;
    // A landed, non-charge hit shoves its TARGET (not the actor) away from whoever just
    // hit it, so the recoil direction is fixed by the seat the target is in, not by who
    // attacked.
    const bool hitLanded = combat.lastDamage() > 0 && !combat.lastWasCharge();
    const int rivalHitBeat = (hitLanded && lastByLocal) ? hitBeat : -1;
    const int localHitBeat = (hitLanded && !lastByLocal) ? hitBeat : -1;
    // Whether the hit that just landed is one the TARGET flinches at — see
    // hurtPoseEarned. Only the authored pose asks; the flash and the nudge below run off
    // the hit beats above and fire for every landed hit exactly as before.
    const bool rivalFlinches = hurtPoseEarned(en, combat.lastDamage());
    const bool localFlinches = hurtPoseEarned(pl, combat.lastDamage());
    // Wind-up charges toward WARN and impact snaps toward INK: the two cues differ in
    // ramp direction AND in hue, and the pair that is live wins on magnitude alone.
    const uint8_t rivalWindup = windupFlashAmt(en.channelMoveIdx >= 0, animBeat);
    const uint8_t localWindup = windupFlashAmt(pl.channelMoveIdx >= 0, animBeat);
    const uint8_t rivalImpact = impactFlashAmt(rivalHitBeat);
    const uint8_t localImpact = impactFlashAmt(localHitBeat);
    // Attacker-forward / target-back hop: any resolved STRIKE, landed or fully absorbed
    // (see attackHopPx above for why one dir/beat pair covers both sprites).
    const bool struck = combat.lastWasStrike();
    const int hopBeat = struck ? hitBeat : -1;
    const int hopDir = lastByLocal ? +1 : -1;
    const int hop = attackHopPx(hopBeat, hopDir);
    // The swing window is the hop's own, so an authored attack clip runs exactly as long
    // as the lunge that carries it — one motion, not a pose that outlives its nudge.
    const bool swinging = struck && hitBeat >= 0 && hitBeat < kAttackHopPeriod;
    // FX_CAMO. A pet holding a move borrowed from another line wears the colours of
    // whoever that move belongs to for as long as it holds it. Both halves are the
    // caller's (CombatCamo) and read straight: the level rather than derived from any
    // beat on this screen, so nothing between the pet's own casts can move it, and the
    // palette rather than sampled here, because which sprite to sample is a question
    // about the fight (camoTarget) that only the caller can answer.
    const uint8_t localCamoAmt = camo.ramp.empty() ? 0 : camo.level;
    const CamoRamp& localCamo = camo.ramp;
    const CamoRamp* localCamoFrom = camo.leaving.empty() ? nullptr : &camo.leaving;
    // The beaten rival's outro takes its seat when one is running — it IS the rival for
    // those beats, so nothing else has to know the fight ended. See CombatOutro
    // (combat_screen.h) for why the two dissolves mean different things.
    const AbsorbPhase outroPhase =
        absorbPhase(outro.beat, kAbsorbLeadBeats, kAbsorbBeats);
    const bool absorbing = outro.kind == CombatOutro::Kind::Absorb;

    // Rival first: where two Daemon cells run right to their band edges, the local pet
    // reads on top of its opponent — and an absorbed rival must pass BEHIND the pet
    // eating it, which is the same ordering.
    if (outro.kind == CombatOutro::Kind::None) {
        drawFighter(fb, rivalSprite, stage.rivalX, animBeat,
                    std::max(rivalWindup, rivalImpact),
                    palColor(rivalImpact >= rivalWindup ? Pal::INK : Pal::WARN),
                    impactNudgePx(rivalHitBeat, +1) + hop, /*faceRight=*/false,
                    fightPose(en,
                              rivalFlinches && rivalHitBeat >= 0 &&
                                  rivalHitBeat < kImpactPeriod,
                              swinging && !lastByLocal),
                    shotN, shotD);
    } else if (rivalSprite) {
        // Same turn the live draw above takes: a rival that spun round on the beat it
        // died would read as the outro doing it, not as the fight ending.
        const bool rivalMirror = spriteMirrorToFace(*rivalSprite, /*faceRight=*/false);
        const int rx = stage.rivalX -
                       scaleUp(spriteContentX0(*rivalSprite, rivalMirror), shotN, shotD);
        const int ry = kSpriteShelf - rivalSprite->h * shotN / shotD;
        if (absorbing) {
            // Into the middle of the local pet's DRAWING, so it is eaten by the body
            // rather than by a corner of an empty cell.
            const int px0 = stage.localX;
            const int py0 =
                kSpriteShelf - (localSprite ? localSprite->h : 0) * shotN / shotD;
            drawAbsorb(fb, *rivalSprite, 0, rx, ry, shotN, shotD,
                       px0 + stage.localW / 2,
                       py0 + (localSprite ? localSprite->h : 0) * shotN / shotD / 2,
                       palColor(Pal::ACCENT), outroPhase.progress, /*bite=*/255,
                       /*row=*/0, rivalMirror);
        } else {
            drawShred(fb, *rivalSprite, 0, rx, ry, shotN, shotD,
                      palColor(Pal::INK), outroPhase.progress, /*row=*/0, rivalMirror);
        }
    }
    drawFighter(fb, localSprite, stage.localX, animBeat,
                absorbing ? std::max(outroPhase.flash,
                                     std::max(localWindup, localImpact))
                          : std::max(localWindup, localImpact),
                absorbing && outroPhase.flash >= std::max(localWindup, localImpact)
                    ? palColor(Pal::ACCENT)
                    : palColor(localImpact >= localWindup ? Pal::INK : Pal::WARN),
                impactNudgePx(localHitBeat, -1) + hop, /*faceRight=*/true,
                fightPose(pl,
                          localFlinches && localHitBeat >= 0 &&
                              localHitBeat < kImpactPeriod,
                          swinging && lastByLocal),
                shotN, shotD, &localCamo, localCamoAmt, localCamoFrom);

    // Worm replicas, on the same shelf, standing BETWEEN their parent and its opponent —
    // each row starts at the parent's own drawn edge facing the other fighter and falls
    // back from there, so a copy is always in the way of the thing it is there to catch.
    // Drawn AFTER both fighters so they read in front of the worm that made them. Nothing
    // is drawn for any other line — wormReplicaCount is 0 and the row returns.
    // (A worm's own sprite is meant to be small enough to leave this room; the stand-in
    // frame the line ships with is not, so the back ranks currently sit over its body.)
    //
    // WormKill names its side in Combat's player_/enemy_ terms, so it is rebound to the
    // local/rival roles the same way everything else on this screen is.
    const WormKill& kill = combat.lastWormKill();
    const bool killOnLocal = kill.onPlayer != flip;
    // The slot pitch is cut from the PARENT'S OWN WIDTH, so a full board occupies the
    // ground its parent stands on instead of sprawling off the back of it.
    //
    // A fixed pitch could not do this at any camera. kReplicaSlotW is 30 active px and
    // three copies step 90 back from the parent's front edge, against a worm whose
    // drawing is 56 — so the last rank stood entirely behind its parent's tail, in empty
    // stage, and the middle one sat over its body. Scaling the pitch with the shot does
    // not help either: the parent scales by the same factor, so the ratio that causes it
    // is invariant under the camera. It is the pitch that is wrong, not the size.
    //
    // Derived from the seat rather than the sprite because the seat is what the fighter
    // visibly occupies. Floored so a very small parent still spreads its copies far
    // enough to be counted, and the back-to-front draw order below already covers the
    // case where that floor makes two ranks touch.
    auto replicaSpan = [&](int bandW) {
        const int fitted = bandW / (kWormReplicaSlots + 1);
        const int floorPx = kReplicaSlotW * shotN / shotD / 2;
        return fitted > floorPx ? fitted : floorPx;
    };
    drawReplicaRow(fb, en, swinging && !lastByLocal, kill, killOnLocal ? -1 : hitBeat,
                   /*frontX=*/stage.rivalX, /*stride=*/replicaSpan(stage.rivalW),
                   kSpriteShelf, animBeat, shotN, shotD);
    drawReplicaRow(fb, pl, swinging && lastByLocal, kill, killOnLocal ? hitBeat : -1,
                   /*frontX=*/stage.localX + stage.localW,
                   /*stride=*/-replicaSpan(stage.localW), kSpriteShelf, animBeat, shotN,
                   shotD);

    // Each fighter's STATUS STRIP, on the shelf under its feet: every condition it is
    // under, as the same glyph the panel's VS grid names that row with, so the two say
    // one thing in one vocabulary. This is what lets the panel stop carrying a drive, a
    // trap pile or the worm copies — none of them was ever hidden, and a row spelling out
    // what the screen is already showing is a row spent twice.
    //
    // Seated at the fighter's own band and clipped to it: a strip that ran past its
    // owner's width would read as belonging to whoever it reached.
    auto statusStrip = [&](const Combatant& c, bool withGuard, int bandX, int bandW) {
        const CombatStatusStrip st = combatStatusStrip(c, withGuard);
        const int cell = kFontAdvance;
        int x = bandX < 0 ? 0 : bandX;                     // an oversized cell may crop
        const int right = bandX + bandW;
        for (int i = 0; i < st.n && x + cell <= right && x + cell <= kActiveW; ++i) {
            if (const SpriteData* g = combatVsGlyph(st.k[i]))
                drawSpriteTinted(fb, *g, glyphFrame(*g, animBeat), x, kSpriteShelf + 1,
                                 combatVsColor(st.k[i]));
            x += cell;
        }
    };
    statusStrip(pl, /*withGuard=*/true, stage.localX, stage.localW);
    statusStrip(en, /*withGuard=*/false, stage.rivalX, stage.rivalW);

    // The strike mark, in the lane the seating reserved for it: who is hitting whom, in
    // the direction the blow travels. Drawn over the replicas, since a copy taking the
    // hit is still that hit landing.
    if (swinging) {
        // The ACTOR decides WHICH mark (what was swung); the fight's swing count decides
        // which half of its pair, so this blow never repeats the one before it.
        const StrikeMark mark =
            strikeMark(lastByLocal ? pl : en, combat.strikeCount());
        drawStrikeMark(fb, stage.laneX, stage.laneW, hopDir, hitBeat, kAttackHopPeriod,
                       *mark.sheet, mark.variant, shotN, shotD);
    }
    // The wind-up countdown, over whichever fighter is charging — the same marker on both
    // sides, so "a hit is being wound up, by that one, N turns out" reads without colour.
    // Seated off each sprite's own height, so it rides the head it belongs to rather than
    // floating on a shared row a short creature never reaches.
    auto headY = [&](const SpriteData* s) {
        return s ? kSpriteShelf - s->h * shotN / shotD : kSpriteShelf;
    };
    // The move being charged is what says how LONG the wind-up is; a combatant mid-channel
    // always has one, and a meter with no total falls back to the turns still to run.
    auto channelTurns = [](const Combatant& c) {
        const int i = c.channelMoveIdx;
        return (i >= 0 && i < static_cast<int>(c.moves.size()) && c.moves[i])
                   ? c.moves[i]->channelTurns : 0;
    };
    if (en.channelMoveIdx >= 0)
        drawWindupMark(fb, stage.rivalX + stage.rivalW / 2, headY(rivalSprite),
                       en.channelLeft, channelTurns(en));
    if (pl.channelMoveIdx >= 0)
        drawWindupMark(fb, stage.localX + stage.localW / 2, headY(localSprite),
                       pl.channelLeft, channelTurns(pl));

    // --- The damage popup, over whoever just took it -----------------------
    // WHERE it appears is the point. Right-aligned on the last-move line, the number sat
    // in the same corner whoever had been hit, so the one cue that says which side is
    // losing carried no answer to it — the reader had to parse the sentence beside it to
    // find out. Floating it off the target's own head says it in the channel a glance
    // uses, on the same hit beat as the flash and the knock-back, so the three cues are
    // one event.
    //
    // A ransomed hit landed in full but moved no Health (combat.h ransomPool), so it gets
    // the word HELD and the pool's green rather than the red of damage actually taken —
    // the tag, not the colour, is what makes the two readable apart in grayscale.
    const int popRise = damagePopRise(hitLanded ? hitBeat : -1);
    if (popRise >= 0) {
        const bool onRival = lastByLocal;         // the target is whoever did NOT swing
        const SpriteData* hitSprite = onRival ? rivalSprite : localSprite;
        const int bandX = onRival ? stage.rivalX : stage.localX;
        const int bandW = onRival ? stage.rivalW : stage.localW;
        const bool held = combat.lastRansomed();
        char dmg[16];
        std::snprintf(dmg, sizeof(dmg), held ? "HELD %d" : "-%d", combat.lastDamage());
        const int w = textWidth(dmg);
        // Held inside the canvas, and clear of the chrome above: a cropping fighter's
        // midpoint can sit past a screen edge, and a tall one's head reaches the header.
        const int x = std::max(kMargin,
                               std::min(kActiveW - kMargin - w, bandX + bandW / 2 - w / 2));
        const int y = std::max(kCombatPanelTop + 2, headY(hitSprite) - 6 - popRise);
        drawText(fb, x, y, dmg, palColor(held ? Pal::CALM : Pal::HOT));
    }

    // --- Player Health: zoned gauge + numeric ------------------------------
    const int phY = kSpriteShelf + 10;
    drawText(fb, kMargin, phY, sides.localLabel, palColor(Pal::INK));
    const int phX = kGaugeX, phW = kGaugeW;   // the rival's column, shared

    // The local pet's passive strip, full size, riding directly above its Health gauge.
    drawPassiveStrip(fb, pl, phX, phY - 7, phW, 5, /*pipW=*/5, /*pipH=*/5, beat);

    const int phPct = pl.maxHealth > 0 ? pl.health * 100 / pl.maxHealth : 0;
    const Zone hz = healthZone(pl.health, pl.maxHealth);
    const bool pulseOn = (beat & 1) == 0;
    drawGauge(fb, phX, phY, phW, 10, phPct, hz, false, pulseOn);
    if (ongoing && localTurnNext) drawTurnTick(fb, phY, 10);
    char hp[16];
    std::snprintf(hp, sizeof(hp), "%d", pl.health);
    drawText(fb, phX + phW + 6, phY, hp, palColor(Pal::INK));

    // --- Last move + damage popup ------------------------------------------
    const int logY = phY + 15;
    if (combat.lastMoveName()[0]) {
        char line[32];
        const char* who = lastByLocal ? "YOU" : en.name;
        if (combat.lastWasCharge())
            std::snprintf(line, sizeof(line), "%s: %s...", who, combat.lastMoveName());
        else
            std::snprintf(line, sizeof(line), "%s: %s", who, combat.lastMoveName());
        drawText(fb, kMargin, logY, line, palColor(Pal::INK_DIM));
    }


    // --- Result beat (win/lose/flee) ---------------------------------------
    // Win/Lose are stated from the LOCAL operator's side. In a duel the guest holds the
    // enemy_ slot, so Combat's own verdict reads inverted there — same fight, same
    // winner, opposite pronoun.
    const char* result = nullptr;
    const bool localLost = sides.localIsEnemySide ? combat.outcome() == Combat::Outcome::Win
                                                  : combat.outcome() == Combat::Outcome::Lose;
    switch (combat.outcome()) {
        case Combat::Outcome::Win:
        case Combat::Outcome::Lose: result = localLost ? "PET OVERWHELMED" : "TARGET CLEARED"; break;
        case Combat::Outcome::Fled: result = "DISENGAGED"; break;
        case Combat::Outcome::Ongoing: break;
    }
    // The banner sits across the sprite region, which is exactly where a beaten rival
    // comes apart — so while a dissolve is running it waits. The verdict is not being
    // withheld: the rival visibly losing IS the verdict, and the words land the moment
    // there is nothing left of it to cover. A fight with no outro (a loss, a duel, the
    // Sim) is unaffected and banners immediately, as it always has.
    const bool dissolving =
        outro.kind != CombatOutro::Kind::None && outro.beat < kAbsorbTotalBeats;
    if (result && !dissolving) {
        const int bannerY = kSpriteShelf - 53;   // centered over the sprite region
        fb.fillRect(0, bannerY, kActiveW, 22, palColor(Pal::TRACK));
        drawText(fb, (kActiveW - textWidth(result)) / 2, bannerY + 7, result,
                 palColor(Pal::INK));
    }

    // Override picker overlay ----------------------------
    // A flat list in three bands: the pet's moves, then any combat-usable items, then
    // the crew Exploit (only while the player belongs to a crew). The cursor
    // (overridePick) runs across the whole list.
    if (combat.overrideOpen()) {
        const int moveN = combat.overrideMoveCount();
        const auto& items = combat.overrideItems();
        const int itemN = static_cast<int>(items.size());
        const int lockN = combat.overrideLockCount();
        const int crewN = combat.overrideCrewRows();
        const int n = moveN + itemN + lockN + crewN;
        const int boxH = 22 + n * 14;
        const int boxY = 40;
        fb.fillRect(8, boxY, kActiveW - 16, boxH, palColor(Pal::TRACK));
        // Named from what is actually in the box (overridePickerHeader), against the room
        // between the text inset and the box's own right edge.
        char head[48];
        overridePickerHeader(head, sizeof(head), itemN > 0, lockN > 0, crewN > 0,
                             (kActiveW - 8) - 16);
        drawText(fb, 16, boxY + 4, head, palColor(Pal::INK));
        // Every row here is the same shape: a NAME from content, and a short fixed TAG
        // right-aligned inside the box. So the name yields to the tag — travelling
        // inside what's left of the row while it's the focused one, clipped when it
        // isn't. Drawn as two independent drawText calls it didn't yield at all, and a
        // long label (a crew Exploit's name is the longest thing this list ever shows)
        // simply ran underneath its own tag.
        auto pickerRow = [&](int y, const char* name, Rgb565 nameCol, const char* tag,
                             bool sel) {
            const int tagX = kActiveW - 24 - textWidth(tag);
            drawText(fb, tagX, y, tag, palColor(Pal::INK_DIM));
            const int room = tagX - kMargin - 24;
            if (room > 0) drawTextMarquee(fb, 24, y, room, name, nameCol, beat, sel);
        };
        for (int i = 0; i < n; ++i) {
            const int y = boxY + 18 + i * 14;
            const bool sel = i == combat.overridePick();
            if (sel) drawRowCursor(fb, 14, y, palColor(Pal::ACCENT));
            const Rgb565 nameC = sel ? palColor(Pal::ACCENT) : palColor(Pal::INK);
            if (i < moveN) {                              // a move row
                const MoveDef* m = combat.player().moves[i];
                // The kind AND the power. What an operator is deciding here is which
                // move to spend the turn on, and the two questions that answers are what
                // KIND of thing it is and how hard it hits — the second of which the row
                // did not carry, so the choice was made blind on the only axis that
                // separates one attack from another. A wildcard becomes whatever it
                // rolls and so has no power to print; it takes the same dash the KIT page
                // uses for a quantity not in play.
                char tag[12];
                if (moveIsWildcard(*m))
                    std::snprintf(tag, sizeof(tag), "%s -", moveKindTag(m->kind));
                else
                    std::snprintf(tag, sizeof(tag), "%s %d", moveKindTag(m->kind),
                                  m->power);
                pickerRow(y, m->displayName, nameC, tag, sel);
            } else if (i < moveN + itemN) {               // a USE-ITEM row
                const OverrideItem& it = items[i - moveN];
                char tag[10];
                std::snprintf(tag, sizeof(tag), "+%d HP", it.heal);
                pickerRow(y, it.label, nameC, tag, sel);
            } else if (i < moveN + itemN + lockN) {       // a LOCK row
                // The move this slot last rolled, tagged as what committing does rather
                // than as what the move is — the kind tag is already on its own row above,
                // and what an operator is deciding here is whether to stop rolling.
                const MoveDef* lm = combat.overrideLockMove(i - moveN - itemN);
                pickerRow(y, lm ? lm->displayName : "-", nameC, "LOCK", sel);
            } else {                                      // the crew Exploit row
                const CrewExploit& ce = combat.overrideCrew();
                char tag[16];
                crewExploitLabel(tag, sizeof(tag), ce.kind, ce.magnitude);
                pickerRow(y, ce.label, nameC, tag, sel);
            }
        }
    }

    // --- Stat panel (B cycles: closed -> VS -> KIT -> closed) ------------
    // A live readout, boxed over the sprites so the always-on chrome stays clean. Hidden
    // while the override picker owns the same space. What each page is FOR, and why the
    // split falls where it does, is on kCombatStatPages in the header.
    //
    // THE PANEL IS 24 CHARACTERS WIDE, and everything below is laid out against that
    // number rather than against a generous snprintf buffer. A row that outgrows it does
    // not fail loudly — the marquee scrolls it and the panel still looks drawn — so the
    // budget is held by gates over the content that reaches these rows rather than by
    // eye. The token half WRAPS onto as many lines as it needs; the rest is columns.
    //
    // THE BOX IS A FIXED RECTANGLE, sized once (kCombatPanelRows) rather than to its
    // contents. A panel that grew and shrank as tokens came and went would redraw at a
    // different size every turn, and a frame of chrome that moves under a reader is worse
    // than one that is sometimes half empty. The size it is fixed AT is the worst case
    // either page can reach: the deepest boss kit the ladder fields, which
    // test_combat_kit_page_holds_the_widest_boss is what holds.
    if (statPage > 0 && !combat.overrideOpen()) {
        // The box hangs from a FIXED TOP and its floor is cut to what the page has to
        // say, capped at kCombatPanelBottom (the shelf).
        //
        // Anchoring the top is what stops the box from moving under a reader: the
        // heading, the rule and every row it has already drawn sit exactly where they
        // sat last turn, and a status arriving mid-fight extends the FLOOR downward —
        // which is where the new row was going to appear anyway. Sizing the whole
        // rectangle to the worst case instead held all of that still by never letting go
        // of the fight: an ordinary encounter is four vitals and nothing else, and the
        // box drawn for a loaded boss buried both fighters to show them.
        //
        // The cap is still the capacity a gate holds
        // (test_combat_kit_page_holds_the_widest_boss): what changes is that a page
        // shorter than the cap stops there rather than drawing air over the stage.
        const int boxY = kCombatPanelTop, pitch = kCombatPanelPitch;
        const int textX = 16, textR = kActiveW - 16;
        const int rows = statPage == 1 ? vsPageRows(pl, en) : kitPageRows(en, prizes);
        const int wanted = kCombatPanelFirstRow + rows * pitch - (pitch - kFontH) + 3;
        const int boxBottom = std::min(kCombatPanelBottom, wanted);
        fb.fillRect(8, boxY, kActiveW - 16, boxBottom - boxY, palColor(Pal::TRACK));
        char head[16];
        std::snprintf(head, sizeof(head), "%s %d/%d",
                      statPage == 1 ? "VS" : "KIT", statPage, kCombatStatPages);
        drawText(fb, textX, boxY + 4, head, palColor(Pal::INK));
        drawText(fb, textR - textWidth("B NEXT"), boxY + 4, "B NEXT",
                 palColor(Pal::INK_DIM));
        fb.fillRect(textX, boxY + 14, textR - textX, 1, palColor(Pal::PAPER));
        int y = kCombatPanelFirstRow;

        // Every row goes through here, and every row can DECLINE to draw: a fight with
        // enough going on to overflow the box must lose its last line rather than
        // overprint the fight underneath it.
        auto row = [&](int indent, const char* s, Pal col) {
            if (y + kFontH > boxBottom) return;
            drawText(fb, textX + indent, y, s, palColor(col));
            y += pitch;
        };
        // A name/value pair inside the PANEL's own margins — drawLabelValue anchors to
        // the screen's, which would hang the value outside this box.
        auto pairRow = [&](int indent, const char* label, Pal lc, const char* value,
                           Pal vc) {
            if (y + kFontH > boxBottom) return;
            const int vx = textR - textWidth(value);
            drawText(fb, vx, y, value, palColor(vc));
            const int room = vx - (textX + indent) - 4;
            if (room > 0)
                drawTextMarquee(fb, textX + indent, y, room, label, palColor(lc), beat,
                                false);
            y += pitch;
        };
        // The hairline between the two fighters' blocks — the same inset rule the header
        // draws, so "these rows belong to that name" is stated by the same mark on both
        // pages rather than by a gap the eye has to measure.
        auto sepRow = [&] {
            if (y + 4 > boxBottom) return;
            y += 2;
            fb.fillRect(textX, y, textR - textX, 1, palColor(Pal::PAPER));
            y += 4;
        };
        // Who a block belongs to, as the operator's own caption for the seat.
        auto whoRow = [&](const char* who, const Combatant& c) {
            char hp[20];
            std::snprintf(hp, sizeof(hp), "HP %d/%d", c.health > 0 ? c.health : 0,
                          c.maxHealth);
            const bool crit = healthZone(c.health, c.maxHealth) == Zone::Critical;
            pairRow(0, who, Pal::INK, hp, crit ? Pal::HOT : Pal::INK);
        };

        if (statPage == 1) {
            // === VS — the head-to-head =====================================
            //
            // One grid, built by combatVsGrid: a tag, then each fighter's value for it.
            // The rows it emits are already in decision order, so a box that runs out
            // loses the least of it — and says so, rather than going quiet.
            const int col2R = textR;                  // the rival's column, hard right
            const int col1R = textR - 9 * kFontAdvance;   // ...and the local pet's
            int dropped = 0;
            auto vsRow = [&](const SpriteData* glyph, Rgb565 glyphCol, const char* label,
                             const char* a, Pal ac, const char* b, Pal bc) {
                if (y + kFontH > boxBottom) { ++dropped; return; }
                // The glyph names the row; the WORD is what draws when there is no art
                // for it, so a missing master leaves a readable panel rather than a
                // column of gaps.
                if (glyph) drawSpriteTinted(fb, *glyph, 0, textX, y, glyphCol);
                else drawText(fb, textX, y, label, palColor(Pal::INK_DIM));
                // A dash, never a blank: a fighter that is not stunned and a stun the
                // panel failed to report must not look the same.
                const char* av = a[0] ? a : "-";
                const char* bv = b[0] ? b : "-";
                drawText(fb, col1R - textWidth(av), y, av,
                         palColor(a[0] ? ac : Pal::INK_DIM));
                drawText(fb, col2R - textWidth(bv), y, bv,
                         palColor(b[0] ? bc : Pal::INK_DIM));
                y += pitch;
            };
            // Whose column is whose, in the operator's own captions for the two seats.
            vsRow(nullptr, 0, "", sides.localLabel, Pal::INK, sides.rivalLabel, Pal::INK);
            const CombatVsGrid g = combatVsGrid(pl, en, /*localGuard=*/true);
            for (int i = 0; i < g.n; ++i) {
                // Health is the only row carrying a zone: a fighter in the Critical band
                // is the one fact here that changes what you do next.
                const bool hp = std::strcmp(g.r[i].tag, "HP") == 0;
                const bool lCrit =
                    hp && healthZone(pl.health, pl.maxHealth) == Zone::Critical;
                const bool rCrit =
                    hp && healthZone(en.health, en.maxHealth) == Zone::Critical;
                vsRow(combatVsGlyph(g.r[i].kind), combatVsColor(g.r[i].kind), g.r[i].tag,
                      g.r[i].local, lCrit ? Pal::HOT : Pal::INK,
                      g.r[i].rival, rCrit ? Pal::HOT : Pal::INK);
            }
            // The armed crew Exploit is the one thing that cannot be a shared row: each
            // side carries a DIFFERENT one, so the tag is per-fighter and there is no
            // column for it to live in. It gets a line of its own, under the grid.
            auto exploitRow = [&](const char* who, const Combatant& c) {
                if (!c.crewExploit.live()) return;
                if (y + kFontH > boxBottom) { ++dropped; return; }
                char ce[16];
                crewExploitLabel(ce, sizeof(ce), c.crewExploit.kind,
                                 c.crewExploit.count());
                char line[40];
                std::snprintf(line, sizeof(line), "%s  %s", who, ce);
                drawText(fb, textX, y, line, palColor(Pal::ACCENT));
                y += pitch;
            };
            if (pl.crewExploit.live() || en.crewExploit.live()) {
                sepRow();
                exploitRow(sides.localLabel, pl);
                exploitRow(sides.rivalLabel, en);
            }
            // Nothing may vanish without saying so. A row that could not be drawn is a
            // fact the operator cannot see, and a panel that simply stopped would look
            // exactly like a fighter with nothing left to report.
            if (dropped > 0) {
                char more[24];
                std::snprintf(more, sizeof(more), "+%d MORE", dropped);
                drawText(fb, textX, boxBottom - kFontH, more, palColor(Pal::WARN));
            }
        } else {
            // === KIT — what the RIVAL can do, and what it is worth ===========
            //
            // The rival's list ONLY. The pet's own moves are on the A+C command picker,
            // which is where they are chosen from and where their powers are already
            // read; repeating them here bought nothing and cost half the box — and a
            // boss fields up to seven moves, so the two lists together overran the panel
            // and the rival's last rows, the ones a player opened this page for, were
            // the ones silently dropped.
            whoRow(sides.rivalLabel, en);
            for (size_t i = 0; i < en.moves.size(); ++i) {
                const MoveDef* m = en.moves[i];
                if (!m) continue;
                // Every row opens with a two-character GUTTER, marked or not, so the kind
                // tags and the names beneath them stay in columns instead of stepping in
                // and out by whether a row happens to be a prize. A marker that moved the
                // text it marks would cost more legibility than it bought.
                //
                // The mark is a glyph rather than a word for width: the row already
                // carries a kind tag, a name and a number inside 24 characters. The
                // legend below is where the word goes, once, instead of on every row.
                const bool learnable = prizes.marked(i);
                char name[44];
                std::snprintf(name, sizeof(name), "%s%s %s", learnable ? "+ " : "  ",
                              moveKindTag(m->kind), m->displayName);
                char pw[8];
                // A wildcard row becomes whatever it rolls, so it HAS no power to print —
                // and a number here would be the one readout on this page that lies. The
                // dash is the same mark the VS page uses for a quantity not in play.
                if (moveIsWildcard(*m)) std::snprintf(pw, sizeof(pw), "-");
                else std::snprintf(pw, sizeof(pw), "%d", m->power);
                // The move it is CHANNELLING is the one thing here that is not a standing
                // fact, and the most urgent — a wind-up is a hit already on its way. It
                // takes the colour when a row is both, which costs the prize nothing: the
                // gutter mark is a separate channel and is still there to be read.
                const bool winding = static_cast<int>(i) == en.channelMoveIdx;
                const Pal nameCol = winding     ? Pal::WARN
                                    : learnable ? Pal::CALM
                                                : Pal::INK_DIM;
                // No row indent: the gutter IS the indent setting these under the name
                // above, and paying for both would spend two characters of a
                // 24-character line saying one thing.
                pairRow(0, name, nameCol, pw, Pal::INK_DIM);
            }
            // The Exploit the rival is CARRYING but has not fired (an arena rival arms
            // its own). The trigger it waits for is deliberately NOT stated: which moment
            // a rival commits to is the read the arena exists to teach.
            if (en.autoExploit.label && !en.autoExploitFired)
                pairRow(16, en.autoExploit.label, Pal::ACCENT,
                        crewExploitTag(en.autoExploit.kind), Pal::ACCENT);
            // What the gutter mark means, spelt out once under the list that uses it —
            // and only when something is marked, so the line answers a question the page
            // has just raised rather than standing as chrome.
            if (prizes.mask) {
                y += 3;                          // a footer of the list, not a row in it
                row(0, "+ WIN TO LEARN", Pal::CALM);
            }
        }
    }

    // Mandatory hint band: A+C live, C reassigned -----------------
    const char* hint = combat.outcome() != Combat::Outcome::Ongoing
                           ? "B CONTINUE"
                       : combat.overrideOpen() ? "A CYCLE B COMMIT C CANCEL"
                       : sides.canRun          ? "A+C CMD B STAT C RUN A SKIP"
                                               : "A+C CMD  B STAT  A SKIP";
    drawHintBand(fb, hint);
}

} // namespace mal
