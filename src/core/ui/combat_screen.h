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

#include <cstddef>
#include <cstdint>

#include "core/render/font.h"

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

// The mid-combat panel's pages. B CYCLES rather than toggles: closed -> VS -> KIT ->
// closed.
//
// The two pages are cut by the QUESTION being asked, not by the kind of data involved.
// An operator opens this panel to decide one more turn, and there are three things they
// can be deciding:
//
//   Can I survive its next hit?   |  both are my numbers against its numbers, so both
//   Can I kill it faster?         |  live on VS, as two columns of the same four rows.
//   Is this worth continuing?     -> what its kit can do, and what beating it teaches.
//
// Splitting instead by data type — everything live on one page, everything standing on
// the other — put each half of the commonest comparison in the game on a different page,
// so the panel could not answer its own first question without a keypress. A number BOTH
// fighters have is a column; a fact about ONE of them is a line under it.
//
// Nothing on either page belongs to the local pet's kit: the A+C command picker already
// lists its moves and their powers, and that is where one is chosen. Repeating them here
// bought a second reading of what the operator is about to see anyway, and cost the room
// a six-move boss needs.
constexpr int kCombatStatPages = 2;

// The VS page, as DATA: one row per thing a fight can be decided by, carrying each
// fighter's value for it. Pure and separate from the draw so a gate can assert that a
// live state reaches the operator, rather than trying to read it back out of pixels.
//
// A GRID rather than two lists, because almost everything here is a quantity BOTH
// fighters can have and at most one of each: one shield, one rot, one stun, one ransom
// bill. Laid out as two lists it read as a heap and, in a loaded fight, overran the box
// so that the second fighter's half was the half that silently vanished. Laid out as
// rows, the same facts cost one row between them instead of one each.
//
// A row appears only when at least one side has it, so an ordinary encounter is the four
// vitals and nothing else. `local`/`rival` empty means "not in play for that fighter",
// which the draw renders as a dash — a fighter that is not stunned and a stun the panel
// forgot to report must not look the same.
//
// What MOVED a vital is folded onto the vital's own row as a signed delta rather than
// being a row of its own: a siphon and a Lockout stack both push on Power, and what an
// operator needs is where Power actually stands and which way it is going. The two
// mechanics are still distinguishable — the effective figure carries the sum, the delta
// carries the movement — and it costs a row instead of three.
// Which row this is, as a VALUE rather than as its own printed name. The draw turns it
// into a glyph and a tint (combatVsGlyph, theme.h's combatVsColor); `tag` is the same
// fact in words, kept because it is the channel that survives when the other two cannot
// be read — a shape is the primary channel and a colour only ever repeats it.
enum class CombatVsKind : uint8_t {
    Health, Power, Defense, Speed, Stun, Dot, Shield, Guard, Ransom, Backup, Trap, Copy
};

struct CombatVsRow {
    CombatVsKind kind = CombatVsKind::Health;
    char tag[6] = {0};
    char local[12] = {0};
    char rival[12] = {0};
};
struct CombatVsGrid {
    // Every distinct row this grid can build. A fighter cannot hold more than a few of
    // them at once (the line-exclusive mechanics are mutually exclusive by construction),
    // but the cap is the vocabulary's size, not a guess at how many can be live.
    static constexpr int kCap = 12;
    CombatVsRow r[kCap];
    int n = 0;
    void push(CombatVsKind kind, const char* tag, const char* a, const char* b);
    bool has(const char* tag) const;
};

// `localGuard` reports the one-shot defend brace, which is only meaningful on the side
// whose braces the operator is choosing; a rival's brace is spent before the panel could
// be opened on it. Rows come out in DECISION order — the four vitals, then what changes
// what you would do next turn, then what merely happened — because the box is finite and
// what falls off the bottom should be the least of it.
CombatVsGrid combatVsGrid(const Combatant& local, const Combatant& rival, bool localGuard);

// What a fighter is UNDER, as a strip of glyphs the combat screen draws beneath it —
// so a condition is visible in the fight itself and not only to someone who opened the
// panel. Countable things repeat their glyph rather than printing a number, the same
// dual-coding the Sealed Cache tiers use (assets/README.md's tinting rule).
//
// This is why the panel does not carry them: a row there would be a second, worse copy
// of what the screen is already showing. What stays on the panel is the things whose
// VALUE is the decision and cannot be counted at a glance — how much rot, for how many
// turns, how big a bill and when it lands.
//
// Worm replicas are absent on purpose: they are already drawn as BODIES on the shelf
// (ui/worm_replicas.h), which is the same fact said better.
struct CombatStatusStrip {
    static constexpr int kCap = 8;
    CombatVsKind k[kCap] = {};
    int n = 0;
};
CombatStatusStrip combatStatusStrip(const Combatant& c, bool withGuard);

// The 8x8 glyph that names a VS row — one FONT CELL, so it sits in a text row without
// changing the row's height, which is the whole reason this tier exists (the icon tiers
// in VISUAL_LANGUAGE.md 3.1 all stand taller than the 11px the panel gives a row, so an
// icon drawn from one of them would cost the rows the grid exists to save).
const SpriteData* combatVsGlyph(CombatVsKind kind);

// The four numbers a fight is decided by, EFFECTIVE — after every siphon, stack and
// clamp the attack path itself applies, so they are what the next exchange will actually
// use rather than what the row was authored with. The same four a pet's own levelling
// spends its points on (kLevelStatCount), which is why these four and not another set.
//
// Split out from the grid above because the grid FORMATS them (a cell, a delta, a dash)
// while these are the values themselves — so anything that wants to reason about a
// fighter's real Power reads a number here rather than parsing a string back out.
struct CombatVitals {
    int health = 0, maxHealth = 0;
    int power = 0;     // powerMultPct + stackPowerBonus, the SUM combat multiplies by
    int defense = 0;   // incoming-damage cut, under the never-immune clamp
    int speed = 0;     // rounded to whole ticks (a siphon steals fractions)
};
CombatVitals combatVitals(const Combatant& c);

// The stat panel's BOX, in active px. Stated here rather than buried in the draw because
// what fits in it is a CONTENT question — a boss's whole kit has to fit on the KIT page,
// and an area that grows one a boss teaches must fail a gate rather than quietly lose the
// rows a player opened the page for. A row declines to draw unless its full glyph height
// clears the bottom, so the capacity is a floor division and not an estimate.
constexpr int kCombatSpriteShelf = 165;                 // where the fighters' feet sit
constexpr int kCombatPanelTop = 28;
// Stops short of the last-move line (the shelf + the local gauge + its own offset): what
// a fighter just DID is the other half of reading a fight, and a panel that covered it
// would trade one blindness for another.
constexpr int kCombatPanelBottom = kCombatSpriteShelf + 10 + 15 - 6;
constexpr int kCombatPanelFirstRow = kCombatPanelTop + 20;   // under the header + its rule
constexpr int kCombatPanelPitch = 11;
constexpr int kCombatPanelRows =
    (kCombatPanelBottom - kFontH - kCombatPanelFirstRow) / kCombatPanelPitch + 1;

// statPage drives that panel: 0 hides it, 1 VS, 2 KIT. It is overlaid over the
// sprites so the always-on chrome stays uncluttered.
// beat paces the gauge's ~1Hz Critical pulse (the shared UI_GAUGE cadence, same as
// every other screen); animBeat is combat's own faster (kCombatAnimMs) tick driving
// sprite motion (idleFrame/windup-flash) so it stays lively at accelerated turn pacing
// without speeding up the gauge pulse too; hitBeat is animBeat-ticks since the last
// landed combat_.step(), driving the post-hit impact punch/flash.
// What becomes of the beaten rival while the result beat is held, before the status
// readout takes the screen. The two dissolves are a VOCABULARY, not decoration:
//
//   Shred  — it comes apart where it stood and gives nothing (core/render/shred.h).
//   Absorb — it streams into the pet, because it fielded a move the pet does not own
//            (core/render/absorb.h). The one worth coming back for.
//
// So the last beat of a fight answers "was that worth farming" without a word of UI.
// Both are dual-coded by SHAPE — lines flying apart against a stream converging on the
// pet — so the distinction survives a grayscale screenshot.
struct CombatOutro {
    enum class Kind : uint8_t { None, Shred, Absorb };
    Kind kind = Kind::None;
    int beat = 0;   // heartbeats since the result landed; drives the sweep
};

// Which of the RIVAL's moves the KIT page marks as a PRIZE — one this pet does not
// have and that beating this rival could teach it. The outro answers the same question
// with a whole-fight yes/no (Absorb vs Shred); this is that answer broken out per move,
// so an operator who sees the pet reach for something can find out what.
//
// A BITMASK over the rival's `moves` by index rather than a re-derived predicate,
// because the filter is the app's (Game::moveIsTeachable — not-owned, and not another
// line's exclusive, exactly as the drop roll reads it) and a screen must never grow a
// lookalike of it. Default-constructed marks nothing, which is the honest reading for
// every fight that cannot teach: a duel, an arena bout, a Sim dummy.
//
// The mask is indexed against the rival BY ROLE, matching `CombatSides` — the seat
// whose caption is `rivalLabel`. The only fight where that is not Combat's own enemy_
// slot is a duel guest, and a duel teaches nothing, so the mask is empty there anyway.
struct RivalPrizes {
    uint32_t mask = 0;   // bit i set => rival move i is one this pet could learn
    bool marked(size_t i) const {
        return i < 32 && (mask & (1u << i)) != 0;
    }
};

void drawCombat(Framebuffer& fb, const Combat& combat,
                const SpriteData* playerSprite, const SpriteData* enemySprite,
                int beat, int animBeat, int hitBeat, int statPage = 0,
                const CombatSides& sides = {}, const CombatOutro& outro = {},
                const RivalPrizes& prizes = {});

} // namespace mal
