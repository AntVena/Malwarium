// combat_screen.h — the combat activity screen, shared by every fight the device runs
// (wild encounter, Sim-Battle, linked duel). Full 224 canvas, its own chrome (no
// carousel/submenu header): rival row (name + level + override pip), rival Health
// (neutral) + numeric + channel wind-up, both combatant sprites (reused SPR_PET_* idle
// frames) seated LOCAL LEFT / RIVAL RIGHT, local zoned Health + numeric, the last-move
// line, and the MANDATORY hint band (A+C live, C reassigned). The damage number floats
// off the head of whoever took it, and an initiative tick marks the side that acts next.
// The A+C override picker overlays when open. All grayscale-legible.
//
// The two Health rows share one gauge column (kCombatGaugeX/W below), so a reader can
// see who is ahead by looking rather than by comparing two numbers.
//
// A duel is not a second screen: it varies only by the CombatSides below, so the two modes
// cannot drift apart in layout or in what a gauge means.
#pragma once

#include <cstddef>
#include <cstdint>

#include "core/render/camo.h"   // CamoRamp — the palette CombatCamo carries
#include "core/render/font.h"
#include "core/render/scene_id.h"   // SceneId — the stage is told where it is
#include "core/ui/layout.h"   // kMargin — the gauge column is stated against the grid

namespace mal {

class Framebuffer;
class Combat;
struct SpriteData;
struct Combatant;
struct AnimClip;

// Which authored clip a fighter shows this tick, or nullptr for a combatant with no
// creature behind it (a sprite-named enemy spec), which the draw reads as the row-0
// breathe loop. TAKING a hit outranks throwing one, so a trade reads as the recoil. A
// creature missing the pose it is asked for falls back to its idle, so a sheet may author
// any subset. `takingHit` means "flinches NOW", not "was damaged" — hurtPoseEarned splits
// the two.
const AnimClip* fightPose(const Combatant& c, bool takingHit, bool swinging);

// How big a single hit has to be, as a percentage of the TARGET's own maxHealth, before it
// answers with its BODY — the authored `hurt` pose and the knock-back that carries it.
// Fighters alternate, so a reaction to any landed damage would fire every other beat and a
// permanently flinching creature has no idle left to flinch out of. Measured against
// maxHealth rather than flat, so the bar asks the same question of a Process form and a
// Daemon with four times the pool.
constexpr int kHeavyHitPctOfMax = 20;

// Is a hit of `damage` a heavy one for `target`, by the bar above? Both BODY cues ask it —
// the authored flinch and the knock-back — so the two are one reaction. The impact flash
// and damage popup do not ask; they fire for every landed hit, so this reserves the
// REACTION without hiding the damage.
bool heavyHit(const Combatant& target, int damage);

// Has `target` earned the flinch for a hit of `damage`? A heavy hit has, and a LOCKED
// fighter always has — there the pose is the state it is stuck in rather than a reaction.
// The SHOVE does not follow the lock: a fighter that cannot act is not being hit harder.
bool hurtPoseEarned(const Combatant& target, int damage);

// The strike mark a fighter's last swing draws: which source's sheet, and which half of
// that source's pair.
//
// Keyed on the MOVE's line, not the creature's, because what is being shown is what was
// SWUNG. So a wild with no line still shows a line's mark when it casts a line's move; a
// Metamorphic pet shows the mark of whatever its wildcard rolled; and a cast with no line
// (the common pool) shows the common mark.
//
// The variant walks the FIGHT's swing count (Combat::strikeCount), so no two blows in a
// row draw the same frame — which is the sequence the player is watching. Counting per
// FIGHTER instead makes two same-line fighters advance in lockstep, so every second pair
// of consecutive attacks repeats.
struct StrikeMark {
    const SpriteData* sheet;
    int variant;
};
StrikeMark strikeMark(const Combatant& actor, int strikeSeq);

// Where the two fighters stand, in active px, and the CLASH LANE between them.
//
// The lane keeps two creatures from merging into one unreadable mass, and is where the
// strike mark is drawn, so it is reserved FIRST and each fighter seated against one of its
// edges. Fixed boxes with the gap left over cannot do this: the widest Daemon draws 96px
// against a 60px box, so the two biggest fighters would meet in the middle. The bands are
// the DRAWING's, not the cell's (SpriteData::contentX0) — a cell is routinely much wider
// than what is drawn in it.
//
// EVERY fight is framed at 1/1 — the creature's authored pixels, one panel pixel each —
// for two reasons. It resamples nothing, where the device's usual x1.75 cannot hold two
// content-full Daemon cells (336px against the stage's 220; at 1/1 the same pair wants
// 192). And it is ONE scale, which is what keeps SIZE meaning STAGE: a per-fight camera
// drew a Daemon at 73px beside a Process at 70. Held at 1/1 a Process is 27-40px and a
// Daemon 73-96 in every fight.
//
// The room that buys is spent on MOTION rather than margin — the lane is wide enough for a
// strike mark to cross, and the lunge and recoil are big enough to see (kAttackHop,
// kImpactNudge). CROPPING survives as a last resort for a pair too wide even at 1/1: it
// cuts at the outer screen edges, and only on whichever fighter is over half the room. No
// pairing in the roster reaches it.
struct CombatStage {
    int localX = 0, localW = 0; // the local pet's drawn band (localX may be negative)
    int rivalX = 0, rivalW = 0; // its rival's
    int laneX = 0, laneW = 0;   // the clash lane, always fully on canvas
    // The stage's scale, as the blitter's num/den. Carried on the stage rather than read
    // from a constant at each draw, so everything seated here — fighters, replicas, a
    // worm's slot pitch, the strike mark, the wind-up marker — is at ONE scale.
    int num = 1, den = 1;
};

// Seat a fight. Either sprite may be null — a fighter with no art still holds a
// standard-cell seat, so the side a missing sprite would have occupied stays empty
// instead of the other fighter drifting into the middle of the stage.
CombatStage combatStage(const SpriteData* localSprite, const SpriteData* rivalSprite);

// A fighter's motion this beat, held inside the canvas. `motion` is its total offset from
// its seat in active px, `bandX`/`bandW` the seat, and `outward` the seat's direction away
// from the clash lane: -1 left, +1 right.
//
// Every cue that moves a struck fighter pushes it OUTWARD, which is where the stage has
// least to give — the widest pairings stand closest to the screen edge, so a flat shove
// would walk a fighter off it. The motion is therefore HELD rather than sized down for the
// worst pairing: a fighter with room takes the full travel and one without takes what it
// has. INWARD motion is never held, the lane being reserved for it.
int heldOnStage(int motion, int bandX, int bandW, int outward);

// Which side of a fight the local operator is on, plus what to call the two of them. The
// screen binds captions, stage seats, the zoned gauge and the WIN banner to the
// LOCAL/RIVAL role rather than to Combat's player_/enemy_ slot. That only bites in a
// linked duel (core/model/pvp_battle.h), where both devices run the SAME deterministic
// fight with the HOST as Combat's player_ — so on the guest's screen Combat's "enemy" is
// its own pet. localIsEnemySide swaps the roles for display and inverts the banner; it
// never touches resolution.
struct CombatSides {
    const char* rivalLabel = "ENEMY";   // caption on the opponent's Health row
    const char* localLabel = "YOU";     // caption on the local pet's Health row
    bool localIsEnemySide = false;      // true on a duel guest — flips WIN/LOSE
    // Whether C still means RUN. False in the two fights there is no running from — a
    // linked duel (quitting would desync the other device) and a ROCK THE DOCK bout —
    // where the input side already makes C inert. The hint band reads it so it stops
    // offering a key that does nothing.
    bool canRun = true;
};

// The mid-combat panel's pages. B CYCLES rather than toggles: closed -> VS -> KIT ->
// closed. The two pages are cut by the QUESTION being asked, not by the kind of data:
//
//   Can I survive its next hit?   |  both are my numbers against its numbers, so both
//   Can I kill it faster?         |  live on VS, as two columns of the same four rows.
//   Is this worth continuing?     -> what its kit can do, and what beating it teaches.
//
// Splitting by data type instead — everything live on one page, everything standing on the
// other — puts each half of the commonest comparison on a different page. A number BOTH
// fighters have is a column; a fact about ONE of them is a line under it.
//
// Nothing on either page belongs to the local pet's kit: the A+C picker already lists its
// moves and powers, and repeating them costs the room a six-move boss needs.
constexpr int kCombatStatPages = 2;

// The VS page, as DATA: one row per thing a fight can be decided by, carrying each
// fighter's value for it. Separate from the draw so a gate can assert that a live state
// reaches the operator rather than reading it back out of pixels.
//
// A GRID rather than two lists, because almost everything here is a quantity BOTH fighters
// can have and at most one of each: one shield, one rot, one stun, one ransom bill. As two
// lists the same facts cost one row each and overran the box in a loaded fight.
//
// A row appears only when at least one side has it, so an ordinary encounter is the four
// vitals. `local`/`rival` empty means "not in play for that fighter", which the draw
// renders as a dash — a fighter that is not stunned and a stun the panel forgot to report
// must not look the same.
//
// What MOVED a vital is folded onto the vital's own row as a signed delta rather than
// getting a row: a siphon and a Lockout stack both push on Power, and what is needed is
// where Power stands and which way it is going. The effective figure carries the sum, the
// delta carries the movement.
//
// `kind` is the row as a VALUE — the draw turns it into a glyph and a tint (combatVsGlyph,
// theme.h's combatVsColor). `tag` is the same fact in words, the channel that survives
// when the other two cannot be read.
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

// What a fighter is UNDER, as a strip of glyphs drawn beneath it, so a condition is
// visible in the fight itself and not only to someone who opened the panel. Countable
// things repeat their glyph rather than printing a number, the same dual-coding the Sealed
// Cache tiers use (assets/README.md).
//
// Which is why the panel does not carry them — what stays there is the things whose VALUE
// is the decision and cannot be counted at a glance: how much rot, for how many turns, how
// big a bill and when it lands. Worm replicas are absent because they are already drawn as
// BODIES on the shelf (ui/worm_replicas.h).
struct CombatStatusStrip {
    static constexpr int kCap = 8;
    CombatVsKind k[kCap] = {};
    int n = 0;
};
CombatStatusStrip combatStatusStrip(const Combatant& c, bool withGuard);

// The 8x8 glyph that names a VS row — one FONT CELL, so it sits in a text row without
// changing the row height. The icon tiers in VISUAL_LANGUAGE.md 3.1 all stand taller than
// the panel's 11px row, so one drawn from them would cost the rows the grid exists to save.
const SpriteData* combatVsGlyph(CombatVsKind kind);

// The four numbers a fight is decided by, EFFECTIVE — after every siphon, stack and clamp
// the attack path applies, so they are what the next exchange will use. The same four a
// pet's levelling spends its points on (kLevelStatCount). Split out from the grid above,
// which FORMATS them, so anything reasoning about a fighter's real Power reads a number
// rather than parsing a string back out.
struct CombatVitals {
    int health = 0, maxHealth = 0;
    int power = 0;     // powerMultPct + stackPowerBonus, the SUM combat multiplies by
    int defense = 0;   // incoming-damage cut, under the never-immune clamp
    int speed = 0;     // rounded to whole ticks (a siphon steals fractions)
};
CombatVitals combatVitals(const Combatant& c);

// The stat panel's BOX, in active px. Stated here rather than buried in the draw because
// what fits in it is a CONTENT question: a boss's whole kit has to fit on the KIT page, and
// an area that grows one must fail a gate rather than quietly lose rows. A row declines to
// draw unless its full glyph height clears the bottom, so the capacity is a floor division.
//
// kCombatSpriteShelf is where the fighters' feet sit — high enough that the tallest cell
// the game can field clears the chrome above, low enough that the pet's own block below
// (status strip, passive strip, gauge and numeric, last-move line) is not packed against
// the hint band. At the 1/1 camera the tallest body is 64px, and the ground line matches.
constexpr int kCombatSpriteShelf = 140;

// The two Health rows, published because they are the screen's CONTRACT: the release gates
// sample these bands to prove a gauge and its blips still read in grayscale, and a gate
// that restated the numbers would be a second copy free to fall out of step with the draw.
// Both rows share one column, so the bars start on one x, run to one width and put their
// numerics on one right edge — which is what lets an operator read who is ahead by looking.
//
// The stage scale below is stated rather than measured again for the same reason: a gate
// sampling a band sized for a different scale would go quiet instead of failing.
constexpr int kCombatStageNum = 1, kCombatStageDen = 1;
// The tallest creature cell the game may field (CONTRIBUTING: max 128x64 logical), at
// stage scale — so the seat band runs kCombatMaxBodyH up from the shelf.
constexpr int kCombatMaxBodyH = 64 * kCombatStageNum / kCombatStageDen;
// The furthest a fighter is ever displaced from its seat: it may be lunging and taking a
// hit on the same beat, so a window that must contain a whole fighter has to allow both.
constexpr int kCombatMaxMotionPx = 32;

constexpr int kCombatGaugeX = kMargin + 5 * kFontAdvance + 12;
constexpr int kCombatGaugeW = 100;
constexpr int kCombatGaugeH = 10;
constexpr int kCombatRivalGaugeY = 18;
constexpr int kCombatLocalGaugeY = kCombatSpriteShelf + 10;
constexpr int kCombatPanelTop = 28;
// Stops just clear of the local pet's Health gauge, so both halves of reading a fight
// survive an open panel: what your pet has left (the gauge and its numeric) and what just
// happened to it (the last-move line). The pet's status and passive strips sit in the few
// px above the gauge, so only a page running its full depth reaches over them — the right
// thing to spend last, being a glance-level readout the VS page can answer in words.
constexpr int kCombatPanelBottom = kCombatLocalGaugeY - 4;
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
// What becomes of the beaten rival while the result beat is held. The two dissolves are a
// VOCABULARY, not decoration:
//
//   Shred  — it comes apart where it stood and gives nothing (core/render/shred.h).
//   Absorb — it streams into the pet, having fielded a move the pet does not own
//            (core/render/absorb.h). The one worth coming back for.
//
// So the last beat answers "was that worth farming" without a word of UI. Both are
// dual-coded by SHAPE — lines flying apart against a stream converging — so the
// distinction survives a grayscale screenshot.
struct CombatOutro {
    enum class Kind : uint8_t { None, Shred, Absorb };
    Kind kind = Kind::None;
    int beat = 0;   // heartbeats since the result landed; drives the sweep
};

// WHOSE colours the local pet's live cast makes it, and how far into them it is
// (FX_CAMO, core/render/camo.h).
//
// A LEVEL, not a window. Every other cue here decays off `hitBeat`, which names the most
// recent strike by EITHER fighter and so lasts exactly one strike — right for a punch,
// wrong for a colour, which must hold while the pet holds what it borrowed. So the level is
// carried by the caller (Game::combatCamoLevel_), eased toward the pet's LIVE cast
// (camoTarget) once per anim tick, and read here as a fact.
//
// The RAMPS are resolved by the caller too (Game::drawCombatScreen), since turning a
// CamoTarget into a palette means sampling a sprite out of the registry. `ramp` is what the
// pet wears at `level`; `leaving` is the palette the un-flipped pixels still hold while one
// borrowed palette dissolves into another (camo.h's `from`), empty for a change that starts
// or ends at the pet's own colours.
struct CombatCamo {
    uint8_t level = 0;
    CamoRamp ramp{};
    CamoRamp leaving{};
};

// Whose colours `c`'s live cast puts it in, against the fighter opposite — the state
// CombatCamo's level eases toward. Gated on the CAST rather than on who is casting,
// following a wildcard slot through to what it rolled: moveAllowedForLine (content/defs.h)
// holds every other line to its own moves, so a cast whose line differs from its caster's
// belongs to a metamorphic pet. A STATE, not a beat — lastMoveIdx and lastRolled are
// rewritten only when THAT fighter acts, so an answer holds across any number of rival
// turns.
//
// THE RIVAL comes first: the pet swings the thing in front of it in that thing's own
// palette. A cast is theirs when the rolled move sits in their kit (matched by id) or
// belongs to their line. THE LINE answers when the roll came from another line's pool and
// the rival has nothing to do with it — the common case in a wild encounter. OWN colours
// are what a roll into the generic roster leaves.
CamoTarget camoTarget(const Combatant& c, const Combatant& rival);

// How many rows the A+C Exploit picker can show at once. A HARD ceiling: the box opens at a
// fixed y and grows downward by a row pitch, and the hint band owns the bottom kHintBandH,
// so the eleventh row prints over the band and the twelfth leaves the screen. The item band
// is unbounded (every combat-usable stack in the bag), so the list is windowed against this
// and the header carries the position.
constexpr int kOverridePickerRows = 10;

// Which of the RIVAL's moves the KIT page marks as a PRIZE — one this pet does not have
// that beating this rival could teach it. The outro answers the same question as a
// whole-fight yes/no (Absorb vs Shred); this is it broken out per move.
//
// A BITMASK over the rival's `moves` by index rather than a re-derived predicate: the
// filter is the app's (Game::moveIsTeachable, exactly as the drop roll reads it) and a
// screen must never grow a lookalike of it. Default-constructed marks nothing, which is the
// honest reading for every fight that cannot teach. Indexed against the rival BY ROLE,
// matching `CombatSides`.
struct RivalPrizes {
    uint32_t mask = 0;   // bit i set => rival move i is one this pet could learn
    bool marked(size_t i) const {
        return i < 32 && (mask & (1u << i)) != 0;
    }
};

// `scene` is WHERE the fight is happening — the area being walked, or wherever the pet
// lives when the fight belongs to no area (core/render/scenes.h). It is this screen's
// BACKGROUND pass, so it goes down before anything else composes onto it, and
// SceneId::None is a real answer: the stage falls back to the plain `paper` field.
void drawCombat(Framebuffer& fb, const Combat& combat,
                const SpriteData* playerSprite, const SpriteData* enemySprite,
                int beat, int animBeat, int hitBeat, int statPage = 0,
                const CombatSides& sides = {}, const CombatOutro& outro = {},
                const RivalPrizes& prizes = {}, const CombatCamo& camo = {},
                SceneId scene = SceneId::None);

} // namespace mal
