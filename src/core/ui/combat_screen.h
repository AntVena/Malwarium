// combat_screen.h — the combat activity screen, shared by every fight the device runs
// (wild encounter, Sim-Battle, linked duel). Full 224 canvas, its own chrome (no
// carousel/submenu header): rival row (name + override pip), rival Health (neutral) +
// channel wind-up, both combatant sprites (reused SPR_PET_* idle frames) seated LOCAL
// LEFT / RIVAL RIGHT, local zoned Health + numeric, the last-move line + damage popup,
// and the MANDATORY hint band (A+C live, C reassigned). The A+C override picker overlays
// when open. All grayscale-legible.
//
// A duel is not a second screen: it varies only by the CombatSides below, so the two
// modes cannot drift apart in layout or in what a gauge means.
#pragma once

namespace mal {

class Framebuffer;
class Combat;
struct SpriteData;

// Which side of a fight the local operator is on, plus what to call the two of them.
//
// The screen binds everything — captions, the left/right stage seats, the zoned gauge,
// the WIN banner — to the LOCAL/RIVAL role rather than to Combat's player_/enemy_ slot.
// That distinction only bites in a linked duel (core/model/pvp_battle.h): both devices
// run the SAME deterministic fight with the HOST seated as Combat's player_, so on the
// GUEST's screen Combat's "enemy" is actually its own pet and Combat's Win verdict means
// its opponent won. Setting localIsEnemySide swaps the roles for display and inverts the
// banner; it never touches resolution. Default-constructed is the ordinary PVE reading.
struct CombatSides {
    const char* rivalLabel = "ENEMY";   // caption on the opponent's Health row
    const char* localLabel = "YOU";     // caption on the local pet's Health row
    bool localIsEnemySide = false;      // true on a duel guest — flips WIN/LOSE
};

// showStats toggles the mid-combat stat panel (B) — a live buff/debuff readout for both
// combatants, overlaid over the sprites so the always-on chrome stays uncluttered.
// beat paces the gauge's ~1Hz Critical pulse (the shared UI_GAUGE cadence, same as
// every other screen); animBeat is combat's own faster (kCombatAnimMs) tick driving
// sprite motion (idleFrame/windup-flash) so it stays lively at accelerated turn pacing
// without speeding up the gauge pulse too; hitBeat is animBeat-ticks since the last
// landed combat_.step(), driving the post-hit impact punch/flash.
void drawCombat(Framebuffer& fb, const Combat& combat,
                const SpriteData* playerSprite, const SpriteData* enemySprite,
                int beat, int animBeat, int hitBeat, bool showStats = false,
                const CombatSides& sides = {});

} // namespace mal
