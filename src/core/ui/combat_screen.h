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
struct Combatant;
struct AnimClip;

// Which authored clip a fighter shows this tick, or nullptr for a combatant with no
// creature behind it (a sprite-named enemy spec) — which the draw reads as the row-0
// breathe loop every single-row sheet already takes.
//
// TAKING a hit outranks throwing one, so a trade reads as the recoil: the impact flash
// and knock-back nudge are already telling that story, and a fighter that appears to
// swing through a blow it is visibly absorbing reads as a dropped frame rather than as
// aggression. A creature missing the pose it is asked for falls back to its idle, so a
// sheet may author any subset of them.
const AnimClip* fightPose(const Combatant& c, bool takingHit, bool swinging);

// Where the two fighters stand, in active px, and the CLASH LANE between them.
//
// A fight is read across a gap: the lane is what keeps two creatures from merging into
// one unreadable mass, and it is where the strike mark that says who is hitting whom is
// drawn. So the lane is reserved FIRST and each fighter is seated against one of its
// edges. Fixed boxes with the gap left over cannot do this — a Daemon cell is 168 active
// px against a 104-px box, so the two biggest fighters meet in the middle.
//
// The bands are the DRAWING's, not the cell's (SpriteData::contentX0), because a cell is
// routinely much wider than what is drawn in it and seating by the cell stands a
// well-padded creature a third of its own width back from where it looks like it is.
//
// A pair too wide for the canvas — two content-full Daemon cells want 336px of the 224
// there are — CROPS at the outer screen edges, and only whichever fighter is over half
// the room does: a creature that fits in its half keeps every column however big its
// opponent is. Losing a tail off the side of the frame reads as a camera held tight on
// the fight; letting the two bodies intersect reads as a bug.
struct CombatStage {
    int localX = 0, localW = 0; // the local pet's drawn band (localX may be negative)
    int rivalX = 0, rivalW = 0; // its rival's
    int laneX = 0, laneW = 0;   // the clash lane, always fully on canvas
};

// Seat a fight. Either sprite may be null — a fighter with no art still holds a
// standard-cell seat, so the side a missing sprite would have occupied stays empty
// instead of the other fighter drifting into the middle of the stage.
CombatStage combatStage(const SpriteData* localSprite, const SpriteData* rivalSprite);

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
    // Whether C still means RUN in this fight. False in the two fights there is no
    // running from — a linked duel (quitting would desync the other device's copy) and
    // a ROCK THE DOCK bout (there is no fleeing a bracket) — where the input side already
    // makes C inert. The hint band reads this so it stops offering a key that does
    // nothing: a hint that lies is worse than no hint.
    bool canRun = true;
};

// The mid-combat panel's pages. B CYCLES rather than toggles: closed -> STATE -> KIT ->
// closed. Two pages because the panel answers two different questions and neither fits
// beside the other — "what is happening to these two right now" (the live leans,
// absorbs and afflictions) and "what can they DO" (the equipped kit and the Exploit
// each is carrying). The second only became worth its own page when opponents started
// arriving with real loadouts (ROCK THE DOCK, game_tourney.cpp): against a malbeast the
// kit was a handful of shared moves, and against a rolled pet it is the whole read.
constexpr int kCombatStatPages = 2;

// The STATE page's readout for one fighter, as an ordered set of short TOKENS: its
// leans (speed, attack power, the siphon and stack deltas moving them, the damage cut)
// then its absorbs and afflictions (a shield pool, a brace, a Backup Drive, a ransom
// bill, traps, replicas, a DoT, a stun, the armed Exploit).
//
// PURE and separate from the draw because the panel is 24 characters wide and this set
// is not: the readout used to be packed into one string and drawn into that box, so the
// moment three of these were live the rest was silently cut — precisely the fight that
// needed reading. The draw now WRAPS the set instead, and a gate can assert directly
// that nothing live goes missing rather than trying to read it back out of pixels.
struct CombatTokens {
    static constexpr int kCap = 16;     // above anything the engine can have live at once
    static constexpr int kLen = 14;     // the widest token ("STK PWR+120") with headroom
    char t[kCap][kLen] = {};
    int n = 0;
    void push(const char* fmt, ...);
    // Whether any token starts with `prefix` — how a gate asks "is the stun reported".
    bool has(const char* prefix) const;
};

// `withGuard` reports the one-shot defend brace, which is only meaningful on the side
// whose braces the operator is choosing (the local pet); an enemy's brace is spent
// before the panel could be opened on it. `leanCount` (may be null) reports how many
// of the returned tokens are LEANS — the draw flushes the two groups separately so
// they stay legible as groups.
CombatTokens combatStateTokens(const Combatant& c, bool withGuard, int* leanCount = nullptr);

// statPage drives that panel: 0 hides it, 1 STATE, 2 KIT. It is overlaid over the
// sprites so the always-on chrome stays uncluttered.
// beat paces the gauge's ~1Hz Critical pulse (the shared UI_GAUGE cadence, same as
// every other screen); animBeat is combat's own faster (kCombatAnimMs) tick driving
// sprite motion (idleFrame/windup-flash) so it stays lively at accelerated turn pacing
// without speeding up the gauge pulse too; hitBeat is animBeat-ticks since the last
// landed combat_.step(), driving the post-hit impact punch/flash.
void drawCombat(Framebuffer& fb, const Combat& combat,
                const SpriteData* playerSprite, const SpriteData* enemySprite,
                int beat, int animBeat, int hitBeat, int statPage = 0,
                const CombatSides& sides = {});

} // namespace mal
