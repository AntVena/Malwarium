#include "core/ui/combat_screen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "tunables.h"        // kLevelDmgReduceMaxPct — the never-immune defence clamp
#include "core/model/combat.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"
#include "core/ui/worm_replicas.h"
#include "generated/assets.h"

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

void drawSpriteCentered(Framebuffer& fb, const SpriteData* s, int boxX, int boxY,
                        int boxW, int boxH, int animBeat, uint8_t flashAmt = 0,
                        int xNudge = 0, const AnimClip* pose = nullptr) {
    if (!s) return;
    const int w = s->frameW * kScaleNum / kScaleDen;
    const int h = s->h * kScaleNum / kScaleDen;
    const int x = boxX + (boxW - w) / 2 + xNudge;
    const int y = boxY + (boxH - h);
    // An authored pose plays its own sheet row in order; without one the breathe
    // heuristic runs on row 0, which is the whole of a single-row sheet.
    const int row = pose ? pose->row : 0;
    const int frame = pose ? pose->frameAt(animBeat) : idleFrame(*s, animBeat);
    if (flashAmt > 0)
        drawSpriteFlash(fb, *s, frame, x, y, kScaleNum, kScaleDen,
                        palColor(Pal::INK), flashAmt, row);
    else
        drawSpriteUpscaled(fb, *s, frame, x, y, kScaleNum, kScaleDen, row);
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

namespace {

// Windup "hit shader" cue (no new art/frames): a channeling combatant's
// silhouette snaps toward ink-white then decays back to normal over
// kWindupFlashPeriod anim-ticks, repeating for as long as the wind-up lasts — a
// charging pulse standing in for "preparing something big" instead of an
// animation.
constexpr int kWindupFlashPeriod = 8;
uint8_t windupFlashAmt(bool channeling, int animBeat) {
    if (!channeling) return 0;
    const int t = animBeat % kWindupFlashPeriod;
    return static_cast<uint8_t>(255 * (kWindupFlashPeriod - t) / kWindupFlashPeriod);
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
// kAttackHopPeriod anim-ticks. Unlike the impact punch above — which needs a landed,
// non-charge hit — this fires on every resolved, non-charge move, so a fully-
// shielded swing still reads as "who just attacked" instead of standing still.
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

void drawCombat(Framebuffer& fb, const Combat& combat,
                const SpriteData* playerSprite, const SpriteData* enemySprite,
                int beat, int animBeat, int hitBeat, bool showStats,
                const CombatSides& sides) {
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
    // Bottom-anchored on the "shelf" (boxY+boxH); a sprite taller than boxH
    // (a Daemon can run up to 64 logical / 112 active px) extends upward past
    // boxY rather than clipping. The shelf sits low (kSpriteShelf) so a tall
    // Daemon has headroom above before it reaches the enemy header/channel
    // row, at the cost of the player block below it — that block is packed
    // tight against the hint band instead of leaving it mid-screen (was 138).
    constexpr int kSpriteShelf = 165;
    // Stage seating: the LOCAL pet holds the LEFT seat and its rival the RIGHT one, so a
    // fight reads left-to-right as "mine, then theirs" — and the roster's fixed top-left
    // light (the roster's fixed top-left key) already turns every sprite that way, so the pets face
    // into the fight from the left rather than out of it. The seat follows the local/
    // rival ROLE (`flip`), never Combat's player_/enemy_ slot, so a duel guest sees its
    // own pet on the left exactly like the host does.
    constexpr int kStageW = 104, kStageH = 84, kStageY = kSpriteShelf - kStageH;
    constexpr int kLocalStageX = 4, kRivalStageX = 116;
    // A landed, non-charge hit shoves its TARGET (not the actor) away from whoever just
    // hit it, so the recoil direction is fixed by the seat the target is in, not by who
    // attacked.
    const bool hitLanded = combat.lastDamage() > 0 && !combat.lastWasCharge();
    const int rivalHitBeat = (hitLanded && lastByLocal) ? hitBeat : -1;
    const int localHitBeat = (hitLanded && !lastByLocal) ? hitBeat : -1;
    const uint8_t rivalFlash =
        std::max(windupFlashAmt(en.channelMoveIdx >= 0, animBeat), impactFlashAmt(rivalHitBeat));
    const uint8_t localFlash =
        std::max(windupFlashAmt(pl.channelMoveIdx >= 0, animBeat), impactFlashAmt(localHitBeat));
    // Attacker-forward / target-back hop: any resolved, non-charge move, hit or not
    // (see attackHopPx above for why one dir/beat pair covers both sprites).
    const bool moveResolved = combat.lastMoveName()[0] != '\0' && !combat.lastWasCharge();
    const int hopBeat = moveResolved ? hitBeat : -1;
    const int hopDir = lastByLocal ? +1 : -1;
    const int hop = attackHopPx(hopBeat, hopDir);
    // The swing window is the hop's own, so an authored attack clip runs exactly as long
    // as the lunge that carries it — one motion, not a pose that outlives its nudge.
    const bool swinging = moveResolved && hitBeat >= 0 && hitBeat < kAttackHopPeriod;
    // Rival first: where two tall Daemons overlap at the centre, the local pet reads on
    // top of its opponent.
    drawSpriteCentered(fb, rivalSprite, kRivalStageX, kStageY, kStageW, kStageH, animBeat,
                       rivalFlash, impactNudgePx(rivalHitBeat, +1) + hop,
                       fightPose(en, rivalHitBeat >= 0 && rivalHitBeat < kImpactPeriod,
                                 swinging && !lastByLocal));
    drawSpriteCentered(fb, localSprite, kLocalStageX, kStageY, kStageW, kStageH, animBeat,
                       localFlash, impactNudgePx(localHitBeat, -1) + hop,
                       fightPose(pl, localHitBeat >= 0 && localHitBeat < kImpactPeriod,
                                 swinging && lastByLocal));

    // Worm replicas, on the same shelf, standing BETWEEN their parent and its opponent —
    // each row starts at the seat edge facing the other fighter and falls back from
    // there, so a copy is always in the way of the thing it is there to catch. Drawn
    // AFTER both fighters so they read in front of the worm that made them. Nothing is
    // drawn for any other line — wormReplicaCount is 0 and the row returns.
    // (A worm's own sprite is meant to be small enough to leave this room; the stand-in
    // frame the line ships with is not, so the back ranks currently sit over its body.)
    //
    // WormKill names its side in Combat's player_/enemy_ terms, so it is rebound to the
    // local/rival roles the same way everything else on this screen is.
    const WormKill& kill = combat.lastWormKill();
    const bool killOnLocal = kill.onPlayer != flip;
    drawReplicaRow(fb, en, swinging && !lastByLocal, kill, killOnLocal ? -1 : hitBeat,
                   /*frontX=*/kRivalStageX, /*stride=*/kReplicaSlotW, kSpriteShelf,
                   animBeat);
    drawReplicaRow(fb, pl, swinging && lastByLocal, kill, killOnLocal ? hitBeat : -1,
                   /*frontX=*/kLocalStageX + kStageW, /*stride=*/-kReplicaSlotW,
                   kSpriteShelf, animBeat);

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
    if (result) {
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

    // --- Stat panel (B toggle) ---------------------------------------------
    // A live readout for both combatants, boxed over the sprites so the always-on chrome
    // stays clean. Hidden while the override picker owns the same space.
    //
    // Each fighter gets a block of up to four rows, and every row is dropped when it has
    // nothing to say, so a plain fight stays short and a loaded one grows into the space
    // rather than truncating. Values are dual-coded (WORD + number) throughout.
    //
    // The offense/defense rows lead with the EFFECTIVE figure and then break out what moved
    // it. This matters because two different mechanics push on the same number from
    // different fields: a Phishing siphon shifts powerMultPct while a Ransomware Lockout
    // stack accumulates in stackPowerBonus, and combat multiplies by the SUM (combat.cpp's
    // applyEffect). Reporting only the siphon delta made a stacked pet look like it was
    // purely losing ground to a drain it was in fact out-earning — and left the whole
    // Lockout/Cipher stacking identity with no readout anywhere on the device.
    if (showStats && !combat.overrideOpen()) {
        const int boxY = 34, boxH = 130, pitch = 11;
        fb.fillRect(8, boxY, kActiveW - 16, boxH, palColor(Pal::TRACK));
        drawText(fb, 16, boxY + 4, "STATS", palColor(Pal::INK));
        drawText(fb, kActiveW - 16 - textWidth("B HIDE"), boxY + 4, "B HIDE",
                 palColor(Pal::INK_DIM));
        int y = boxY + 18;
        auto row = [&](int indent, const char* s, Pal col) {
            drawText(fb, 16 + indent, y, s, palColor(col));
            y += pitch;
        };
        auto block = [&](const char* who, const Combatant& c, bool withGuard) {
            char s[40];
            // Vitals. Speed is float (a siphon steals fractional amounts); round to the
            // nearest whole tick — a decimal point would break the tabular-digit convention.
            int off = std::snprintf(s, sizeof(s), "%-5s HP %d/%d  SPD %d", who, c.health,
                                    c.maxHealth, static_cast<int>(std::lround(c.speed)));
            if (c.speed != c.baseSpeed)
                std::snprintf(s + off, sizeof(s) - off, " %+d",
                              static_cast<int>(std::lround(c.speed - c.baseSpeed)));
            row(0, s, Pal::INK);

            // Offense: effective attack multiplier, then its two movers.
            off = std::snprintf(s, sizeof(s), "PWR %d", c.powerMultPct + c.stackPowerBonus);
            if (c.powerMultPct != c.basePowerMultPct)
                off += std::snprintf(s + off, sizeof(s) - off, "  SIPH %+d",
                                     c.powerMultPct - c.basePowerMultPct);
            if (c.stackPowerBonus > 0)
                std::snprintf(s + off, sizeof(s) - off, "  STK +%d", c.stackPowerBonus);
            row(8, s, Pal::INK_DIM);

            // Defense: the effective incoming-damage cut, under the never-immune clamp the
            // attack path applies, plus the Cipher-track stack feeding it.
            int cut = c.dmgReducePct + c.stackDefenseBonus;
            if (cut > kLevelDmgReduceMaxPct) cut = kLevelDmgReduceMaxPct;
            if (cut > 0) {
                off = std::snprintf(s, sizeof(s), "DEF %d", cut);
                if (c.stackDefenseBonus > 0)
                    std::snprintf(s + off, sizeof(s) - off, "  STK +%d", c.stackDefenseBonus);
                row(8, s, Pal::INK_DIM);
            }

            // Absorbs and afflictions: what is standing between this pet and its Health,
            // and what is eating it. RNSM is the numeric half of the green strip on the
            // gauges — the pool and the turns left before it lands.
            off = 0;
            if (c.shieldHp > 0)
                off += std::snprintf(s + off, sizeof(s) - off, "SHLD %d", c.shieldHp);
            // The Backup Drive carried INTO this fight (combat.h itemShield): still
            // holding, or already spent. The two are read from DIFFERENT fields because
            // spending the drive clears itemShield — testing that flag for both states
            // is why USED never used to reach the screen. The idle habitat badges the
            // drive before the fight (game_render.cpp); this is where it reads once the
            // fight it was bought for is actually happening.
            if (c.itemShield || c.backupUse != Combatant::BackupUse::None)
                off += std::snprintf(s + off, sizeof(s) - off, "%sBKUP %s",
                                     off ? "  " : "",
                                     c.itemShield ? "RDY" : "USED");
            if (withGuard && c.guard > 0)
                off += std::snprintf(s + off, sizeof(s) - off, "%sGRD %d",
                                     off ? "  " : "", c.guard);
            if (c.ransomPool > 0)
                off += std::snprintf(s + off, sizeof(s) - off, "%sRNSM %d/%d",
                                     off ? "  " : "", c.ransomPool, c.ransomTurnsLeft);
            if (c.trojanTrapCount > 0)
                off += std::snprintf(s + off, sizeof(s) - off, "%sTRAP %d",
                                     off ? "  " : "", c.trojanTrapCount);
            if (c.dotTurnsLeft > 0)
                off += std::snprintf(s + off, sizeof(s) - off, "%sDOT %dx%d",
                                     off ? "  " : "", c.dotPerTurn, c.dotTurnsLeft);
            if (c.lockedTurnsLeft > 0)
                off += std::snprintf(s + off, sizeof(s) - off, "%sSTUN %d",
                                     off ? "  " : "", c.lockedTurnsLeft);
            // The armed crew Exploit and whatever it still has left — charges, turns,
            // or nothing at all for a kind that simply holds (CrewExploitState::live).
            if (c.crewExploit.live()) {
                char ce[16];
                crewExploitLabel(ce, sizeof(ce), c.crewExploit.kind,
                                 c.crewExploit.count());
                off += std::snprintf(s + off, sizeof(s) - off, "%s%s",
                                     off ? "  " : "", ce);
            }
            if (off > 0) row(8, s, Pal::INK_DIM);
        };
        block("YOU", pl, true);
        y += 4;                       // a breath between the two fighters' blocks
        block("RIVAL", en, false);
    }

    // Mandatory hint band: A+C live, C reassigned -----------------
    const char* hint = combat.outcome() != Combat::Outcome::Ongoing
                           ? "B CONTINUE"
                       : combat.overrideOpen() ? "A CYCLE B COMMIT C CANCEL"
                                               : "A+C CMD B STAT C RUN A SKIP";
    drawHintBand(fb, hint);
}

} // namespace mal
