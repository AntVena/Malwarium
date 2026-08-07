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
                        int xNudge = 0) {
    if (!s) return;
    const int w = s->frameW * kScaleNum / kScaleDen;
    const int h = s->h * kScaleNum / kScaleDen;
    const int x = boxX + (boxW - w) / 2 + xNudge;
    const int y = boxY + (boxH - h);
    if (flashAmt > 0)
        drawSpriteFlash(fb, *s, idleFrame(*s, animBeat), x, y, kScaleNum, kScaleDen,
                        palColor(Pal::INK), flashAmt);
    else
        drawSpriteUpscaled(fb, *s, idleFrame(*s, animBeat), x, y, kScaleNum, kScaleDen);
}

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

// The passive strip — one combatant's live line-passive state as a bar plus a pip row,
// drawn immediately outside its Health gauge. Both fighters get one: a passive changes who
// wins, so hiding the opponent's would leave the player watching a fight decided by
// something they can't see (in a duel, by the pet they're fighting). The rival's is drawn
// smaller, which is the whole reason this takes its sizes as parameters — same language,
// less weight, because the pet you're steering is the one you act on.
//
// A pet has exactly ONE line, so the three states below are mutually exclusive in practice
// and share the strip rather than stacking rows: a Phishing shield, a Trojan trap stack,
// and a Ransomware pool never co-occur on the same combatant.
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
    // Rival first: where two tall Daemons overlap at the centre, the local pet reads on
    // top of its opponent.
    drawSpriteCentered(fb, rivalSprite, kRivalStageX, kStageY, kStageW, kStageH, animBeat,
                       rivalFlash, impactNudgePx(rivalHitBeat, +1) + hop);
    drawSpriteCentered(fb, localSprite, kLocalStageX, kStageY, kStageW, kStageH, animBeat,
                       localFlash, impactNudgePx(localHitBeat, -1) + hop);

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
        for (int i = 0; i < n; ++i) {
            const int y = boxY + 18 + i * 14;
            const bool sel = i == combat.overridePick();
            if (sel) drawRowCursor(fb, 14, y, palColor(Pal::ACCENT));
            const Rgb565 nameC = sel ? palColor(Pal::ACCENT) : palColor(Pal::INK);
            if (i < moveN) {                              // a move row
                const MoveDef* m = combat.player().moves[i];
                drawText(fb, 24, y, m->displayName, nameC);
                drawText(fb, kActiveW - 24 - textWidth(moveKindTag(m->kind)), y,
                         moveKindTag(m->kind), palColor(Pal::INK_DIM));
            } else if (i < moveN + itemN) {               // a USE-ITEM row
                const OverrideItem& it = items[i - moveN];
                drawText(fb, 24, y, it.label, nameC);
                char tag[10];
                std::snprintf(tag, sizeof(tag), "+%d HP", it.heal);
                drawText(fb, kActiveW - 24 - textWidth(tag), y, tag,
                         palColor(Pal::INK_DIM));
            } else {                                      // the crew Exploit row
                const CrewExploit& ce = combat.overrideCrew();
                drawText(fb, 24, y, ce.label, nameC);
                char tag[16];
                std::snprintf(tag, sizeof(tag), "%s x%d", crewExploitTag(ce.kind),
                              ce.magnitude);
                drawText(fb, kActiveW - 24 - textWidth(tag), y, tag,
                         palColor(Pal::INK_DIM));
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
            if (c.crewExploit.charges > 0)   // armed crew-Exploit charges
                off += std::snprintf(s + off, sizeof(s) - off, "%s%s %d",
                                     off ? "  " : "", crewExploitTag(c.crewExploit.kind),
                                     c.crewExploit.charges);
            if (off > 0) row(8, s, Pal::INK_DIM);
        };
        block("YOU", pl, true);
        y += 4;                       // a breath between the two fighters' blocks
        block("RIVAL", en, false);
    }

    // Mandatory hint band: A+C live, C reassigned -----------------
    const char* hint = combat.outcome() != Combat::Outcome::Ongoing
                           ? "B CONTINUE"
                       : combat.overrideOpen() ? "A CYCLE  B COMMIT  C CANCEL"
                                               : "A+C CMD  B STATS  C FLEE  A SKIP";
    fb.fillRect(0, kActiveH - 16, kActiveW, 16, palColor(Pal::TRACK));
    drawText(fb, (kActiveW - textWidth(hint)) / 2, kActiveH - 12, hint,
             palColor(Pal::INK));
}

} // namespace mal
