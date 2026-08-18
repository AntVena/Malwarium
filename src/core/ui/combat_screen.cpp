#include "core/ui/combat_screen.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

#include "tunables.h"        // kLevelDmgReduceMaxPct — the never-immune defence clamp
#include "core/model/combat.h"
#include "core/render/absorb.h"
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
// The outer inset a fighter is seated against before it is allowed to crop, and the
// bounds on the clash lane: never tighter than the strike mark drawn in it, never so
// wide that two small creatures read as ignoring each other.
constexpr int kStageEdge = 2, kLaneMin = 26, kLaneMax = 58;

// Where a sheet column lands in active px under the creature upscale, as the BLITTER
// puts it: the first destination column whose source sample has reached `x`. Seating
// works in these, not in a plain x*num/den, because two independently truncated
// endpoints can bracket a band one pixel wider than the band itself — and one stray
// column is enough to put a creature inside the lane that exists to keep it out.
constexpr int scaleUp(int x) {
    return (x * kScaleNum + kScaleDen - 1) / kScaleDen;
}

// Seat one fighter's drawing so its band starts at `contentX`, bottom-anchored on the
// shelf. The FRAME is placed from there — a sprite padded inside its cell hangs its
// padding outside the band, which is the whole point of seating by content.
void drawFighter(Framebuffer& fb, const SpriteData* s, int contentX, int animBeat,
                 uint8_t flashAmt, Rgb565 flashColor, int xNudge,
                 const AnimClip* pose) {
    if (!s) return;
    const int x = contentX - scaleUp(spriteContentX0(*s)) + xNudge;
    const int y = kSpriteShelf - s->h * kScaleNum / kScaleDen;
    // An authored pose plays its own sheet row in order; without one the breathe
    // heuristic runs on row 0, which is the whole of a single-row sheet.
    const int row = pose ? pose->row : 0;
    const int frame = pose ? pose->frameAt(animBeat) : idleFrame(*s, animBeat);
    if (flashAmt > 0)
        drawSpriteFlash(fb, *s, frame, x, y, kScaleNum, kScaleDen, flashColor,
                        flashAmt, row);
    else
        drawSpriteUpscaled(fb, *s, frame, x, y, kScaleNum, kScaleDen, row);
}

// The seat a fighter with no sprite holds: one standard pet cell (the 56x48 logical
// creature cell), so the stage keeps its shape rather than collapsing around whichever
// side has art.
int seatWidth(const SpriteData* s) {
    constexpr int kPetCellW = 56;
    if (!s) return scaleUp(kPetCellW);
    return scaleUp(spriteContentX1(*s)) - scaleUp(spriteContentX0(*s));
}

}  // namespace

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
    const int wL = seatWidth(localSprite), wR = seatWidth(rivalSprite);
    // The lane takes whatever the two fighters leave, held between its bounds.
    int lane = kActiveW - 2 * kStageEdge - wL - wR;
    lane = std::max(kLaneMin, std::min(kLaneMax, lane));

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
    return st;
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

// The strike mark: WHO is hitting WHOM, drawn in the clash lane for the swing window.
//
// Three swept gashes that bow, fan and TRAVEL in the direction of the blow — crossing
// the lane from the attacker's edge to its target's over the window, thinning to a point
// at both ends the way a claw leaves a cut. Motion carries the direction first and the
// taper carries it second, so the answer survives a single frozen frame as well as it
// does in play. Drawn from primitives rather than art because the mark has to mirror,
// and a mirrored sheet is a second sheet.
//
// It cuts across torso height — high enough to cross a short creature's body rather than
// its feet, low enough to stay under a tall one's head.
constexpr int kStrikeY = kSpriteShelf - 46;
constexpr int kStrikeW = 18, kStrikeGashes = 3;
void drawStrikeMark(Framebuffer& fb, int laneX, int laneW, int dir, int beat,
                    int period) {   // dir: +1 the blow travels right, -1 left
    if (beat < 0 || beat >= period) return;
    const int travel = std::max(0, laneW - kStrikeW);
    const int from = dir > 0 ? laneX : laneX + travel;
    const int x = from + dir * travel * beat / std::max(1, period - 1) + kStrikeW / 2;
    const uint8_t a = static_cast<uint8_t>(255 * (period - beat) / period);
    const Rgb565 col = palColor(Pal::INK);
    for (int g = 0; g < kStrikeGashes; ++g) {
        const int half = g == 1 ? 16 : 11;         // the middle gash leads the claw
        const int gx = x + dir * (g - 1) * 5;
        const int gy = kStrikeY + (g - 1) * 6;
        for (int t = -half; t <= half; ++t) {
            // The gash bows toward the blow and thickens through its belly, so each
            // stroke is a crescent with two points rather than a bar with two ends.
            const int fall = half * half - t * t;
            const int bow = 6 * fall / (half * half);
            const int body = 1 + 3 * fall / (half * half);
            for (int k = 0; k < body; ++k)
                fb.blendPixel(gx + dir * (bow + k), gy + t, col, a);
        }
    }
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
                    int shelfY, int animBeat) {
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
        drawReplica(fb, s, frame, cx, shelfY);
    }
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
    num(a, sizeof(a), local.lockedTurnsLeft, local.lockedTurnsLeft > 0);
    num(b, sizeof(b), rival.lockedTurnsLeft, rival.lockedTurnsLeft > 0);
    pair(CombatVsKind::Stun, "STUN", local.lockedTurnsLeft > 0 || rival.lockedTurnsLeft > 0);

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

    // --- What is merely standing there ----------------------------------------------
    auto bkupCell = [](char* out, size_t cap, const Combatant& c) {
        // Read from DIFFERENT fields because spending the drive clears itemShield, so
        // testing one flag for both states would never render USED at all.
        if (c.itemShield) std::snprintf(out, cap, "RDY");
        else if (c.backupUse != Combatant::BackupUse::None) std::snprintf(out, cap, "USED");
        else out[0] = '\0';
    };
    bkupCell(a, sizeof(a), local);
    bkupCell(b, sizeof(b), rival);
    pair(CombatVsKind::Backup, "BKUP", a[0] || b[0]);

    num(a, sizeof(a), local.trojanTrapCount, local.trojanTrapCount > 0);
    num(b, sizeof(b), rival.trojanTrapCount, rival.trojanTrapCount > 0);
    pair(CombatVsKind::Trap, "TRAP", local.trojanTrapCount > 0 || rival.trojanTrapCount > 0);

    num(a, sizeof(a), local.wormReplicaCount, local.wormReplicaCount > 0);
    num(b, sizeof(b), rival.wormReplicaCount, rival.wormReplicaCount > 0);
    pair(CombatVsKind::Copy, "COPY", local.wormReplicaCount > 0 || rival.wormReplicaCount > 0);
    return g;
}

void drawCombat(Framebuffer& fb, const Combat& combat,
                const SpriteData* playerSprite, const SpriteData* enemySprite,
                int beat, int animBeat, int hitBeat, int statPage,
                const CombatSides& sides, const CombatOutro& outro,
                const RivalPrizes& prizes) {
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

    // --- Enemy header: name + override pip -----------------------------------
    // Difficulty pips already showed once, on the pre-fight encounter intro
    // (drawEncounterIntro, expl_screen.cpp) — repeating them here just ate a
    // full header row for no new information, so this HUD skips them.
    drawText(fb, kMargin, kTitleY, en.name, palColor(Pal::INK));

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
    drawText(fb, hintX - 4 - textWidth("A+C"), 6, "A+C", palColor(Pal::INK_DIM));

    // --- Enemy Health (neutral — emptying is good) + level + channel wind-up ---
    // This row reuses the y-band the encounter-intro header gives its difficulty pips.
    drawText(fb, kMargin, 19, sides.rivalLabel, palColor(Pal::INK_DIM));
    const int ehX = kMargin + textWidth(sides.rivalLabel) + 6;
    const int ehW = 96;
    const float ef = en.maxHealth > 0
                         ? static_cast<float>(en.health) / en.maxHealth : 0.0f;
    drawProgressBar(fb, ehX, 18, ehW, 10, ef, palColor(Pal::ACCENT));
    // level readout: "???" for Sim dummies/bosses (never level-tagged) so
    // the gap reads as "unranked", never as a misleading "Lv 0". Rides at the end
    // of the health row now that the pips row above it is gone.
    char lvBuf[8];
    if (en.hasLevel) std::snprintf(lvBuf, sizeof(lvBuf), "Lv %d", en.level);
    else std::snprintf(lvBuf, sizeof(lvBuf), "Lv ???");
    drawText(fb, ehX + ehW + 8, 19, lvBuf, palColor(Pal::INK_DIM));
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
    // fight reads left-to-right as "mine, then theirs" — and the roster's fixed top-left
    // key light already turns every sprite that way, so the pets face into the fight from
    // the left rather than out of it. The seat follows the local/rival ROLE (`flip`),
    // never Combat's player_/enemy_ slot, so a duel guest sees its own pet on the left
    // exactly like the host does. Bottom-anchored on the shelf; a sprite taller than the
    // band extends upward rather than clipping. Both bands and the lane between them come
    // from combatStage() — see combat_screen.h for why the lane is reserved first.
    const CombatStage stage = combatStage(localSprite, rivalSprite);
    // A landed, non-charge hit shoves its TARGET (not the actor) away from whoever just
    // hit it, so the recoil direction is fixed by the seat the target is in, not by who
    // attacked.
    const bool hitLanded = combat.lastDamage() > 0 && !combat.lastWasCharge();
    const int rivalHitBeat = (hitLanded && lastByLocal) ? hitBeat : -1;
    const int localHitBeat = (hitLanded && !lastByLocal) ? hitBeat : -1;
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
                    impactNudgePx(rivalHitBeat, +1) + hop,
                    fightPose(en, rivalHitBeat >= 0 && rivalHitBeat < kImpactPeriod,
                              swinging && !lastByLocal));
    } else if (rivalSprite) {
        const int rx = stage.rivalX - scaleUp(spriteContentX0(*rivalSprite));
        const int ry = kSpriteShelf - rivalSprite->h * kScaleNum / kScaleDen;
        if (absorbing) {
            // Into the middle of the local pet's DRAWING, so it is eaten by the body
            // rather than by a corner of an empty cell.
            const int px0 = stage.localX;
            const int py0 = kSpriteShelf - (localSprite ? localSprite->h : 0) *
                                               kScaleNum / kScaleDen;
            drawAbsorb(fb, *rivalSprite, 0, rx, ry, kScaleNum, kScaleDen,
                       px0 + stage.localW / 2,
                       py0 + (localSprite ? localSprite->h : 0) * kScaleNum /
                                 kScaleDen / 2,
                       palColor(Pal::ACCENT), outroPhase.progress, /*bite=*/255);
        } else {
            drawShred(fb, *rivalSprite, 0, rx, ry, kScaleNum, kScaleDen,
                      palColor(Pal::INK), outroPhase.progress);
        }
    }
    drawFighter(fb, localSprite, stage.localX, animBeat,
                absorbing ? std::max(outroPhase.flash,
                                     std::max(localWindup, localImpact))
                          : std::max(localWindup, localImpact),
                absorbing && outroPhase.flash >= std::max(localWindup, localImpact)
                    ? palColor(Pal::ACCENT)
                    : palColor(localImpact >= localWindup ? Pal::INK : Pal::WARN),
                impactNudgePx(localHitBeat, -1) + hop,
                fightPose(pl, localHitBeat >= 0 && localHitBeat < kImpactPeriod,
                          swinging && lastByLocal));

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
    drawReplicaRow(fb, en, swinging && !lastByLocal, kill, killOnLocal ? -1 : hitBeat,
                   /*frontX=*/stage.rivalX, /*stride=*/kReplicaSlotW, kSpriteShelf,
                   animBeat);
    drawReplicaRow(fb, pl, swinging && lastByLocal, kill, killOnLocal ? hitBeat : -1,
                   /*frontX=*/stage.localX + stage.localW, /*stride=*/-kReplicaSlotW,
                   kSpriteShelf, animBeat);

    // The strike mark, in the lane the seating reserved for it: who is hitting whom, in
    // the direction the blow travels. Drawn over the replicas, since a copy taking the
    // hit is still that hit landing.
    if (swinging)
        drawStrikeMark(fb, stage.laneX, stage.laneW, hopDir, hitBeat, kAttackHopPeriod);
    // The wind-up countdown, over whichever fighter is charging — the same marker on both
    // sides, so "a hit is being wound up, by that one, N turns out" reads without colour.
    // Seated off each sprite's own height, so it rides the head it belongs to rather than
    // floating on a shared row a short creature never reaches.
    auto headY = [](const SpriteData* s) {
        return s ? kSpriteShelf - s->h * kScaleNum / kScaleDen : kSpriteShelf;
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

    // --- Player Health: zoned gauge + numeric ------------------------------
    const int phY = kSpriteShelf + 10;
    drawText(fb, kMargin, phY, sides.localLabel, palColor(Pal::INK));
    const int phX = kMargin + textWidth(sides.localLabel) + 6;
    const int phW = 110;

    // The local pet's passive strip, full size, riding directly above its Health gauge.
    drawPassiveStrip(fb, pl, phX, phY - 7, phW, 5, /*pipW=*/5, /*pipH=*/5, beat);

    const int phPct = pl.maxHealth > 0 ? pl.health * 100 / pl.maxHealth : 0;
    const Zone hz = healthZone(pl.health, pl.maxHealth);
    const bool pulseOn = (beat & 1) == 0;
    drawGauge(fb, phX, phY, phW, 10, phPct, hz, false, pulseOn);
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
        if (combat.lastDamage() > 0) {
            // A ransomed hit landed in full but moved no Health (combat.h ransomPool), so
            // it gets the word HELD and the pool's green rather than the red of damage
            // actually taken — the tag, not the colour, is what makes the two readable
            // apart in grayscale.
            const bool held = combat.lastRansomed();
            char dmg[16];
            std::snprintf(dmg, sizeof(dmg), held ? "HELD %d" : "-%d", combat.lastDamage());
            drawText(fb, kActiveW - kMargin - textWidth(dmg), logY, dmg,
                     palColor(held ? Pal::CALM : Pal::HOT));
        }
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
        const int crewN = combat.overrideCrewRows();
        const int n = moveN + itemN + crewN;
        const int boxH = 22 + n * 14;
        const int boxY = 40;
        fb.fillRect(8, boxY, kActiveW - 16, boxH, palColor(Pal::TRACK));
        drawText(fb, 16, boxY + 4,
                 crewN ? "EXPLOIT: MOVE/ITEM/CREW" : "EXPLOIT: MOVE / ITEM",
                 palColor(Pal::INK));
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
                pickerRow(y, m->displayName, nameC, moveKindTag(m->kind), sel);
            } else if (i < moveN + itemN) {               // a USE-ITEM row
                const OverrideItem& it = items[i - moveN];
                char tag[10];
                std::snprintf(tag, sizeof(tag), "+%d HP", it.heal);
                pickerRow(y, it.label, nameC, tag, sel);
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
        // The box stops short of the last-move line (logY) rather than running to the
        // foot: what a fighter just DID is the other half of reading a fight, and a
        // panel that covered it would trade one blindness for another.
        // The box is a FIXED rectangle rather than one sized to its contents. A panel
        // that grew and shrank as tokens came and went would redraw at a different size
        // every turn, and a frame of chrome that moves under a reader is worse than one
        // that is sometimes half empty.
        const int boxY = kCombatPanelTop, boxBottom = kCombatPanelBottom,
                  pitch = kCombatPanelPitch;
        const int textX = 16, textR = kActiveW - 16;
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
                std::snprintf(pw, sizeof(pw), "%d", m->power);
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
