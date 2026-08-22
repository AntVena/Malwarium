// test_camo.cpp — native gates for FX_CAMO, the sweep that repaints the pet in another
// line's colours while it swings a move borrowed from that line (core/render/camo.h).
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these.
#include "test_gates.h"

#include <algorithm>

#include "core/model/combat.h"
#include "core/render/camo.h"
#include "core/ui/combat_screen.h"

namespace {

// A 4x1 strip of four known greys on transparent — four distinct luminances in a known
// order, which is the only property the ramp and the remap actually care about.
struct Steps {
    uint16_t rgb[4];
    uint8_t a[4] = {255, 255, 255, 255};
    SpriteData s{};
    Steps() {
        rgb[0] = rgb565(0, 0, 0);
        rgb[1] = rgb565(80, 80, 80);
        rgb[2] = rgb565(170, 170, 170);
        rgb[3] = rgb565(255, 255, 255);
        s.sheetW = 4; s.h = 1; s.frameW = 4; s.frames = 1; s.rows = 1;
        s.rgb = rgb; s.a = a;
    }
};

}  // namespace

// --- Gate: a sampled ramp is that sprite's own colours, ranked dark to light ---
//
// Ranking is the whole reason this works: the worn palette is indexed by the luminance
// of the pixel it replaces, so if the ramp were not a value scale a lit crown could be
// repainted with a core shadow and the creature's form would invert.
void test_camo_ramp_is_a_value_scale() {
    Steps g;
    // Four unique colours is too plain to lend a palette, so the ramp is DERIVED from
    // the sprite's main colour instead — a full ladder either way, which is the whole
    // rule: the pet is never left half-painted.
    const CamoRamp r = camoRampFrom(g.s);
    CHECK(r.count == kCamoRampMax);
    for (int i = 1; i < r.count; ++i)
        CHECK(luminance(r.tone[i - 1]) <= luminance(r.tone[i]));

    // A 1-bit mask has one colour by construction. That ink is the main colour, and the
    // ladder built from it is what turns a pet camouflaged as a Worm GREYSCALE — the
    // line is drawn in one white ink, so wearing it keeps every bit of the pet's own
    // shading and drains the hue out of it, rather than flattening it or flecking it.
    uint8_t bits[1] = {0x80};
    SpriteData m{};
    m.sheetW = 8; m.h = 1; m.frameW = 8; m.frames = 1; m.rows = 1;
    m.bits = bits; m.ink = rgb565(240, 240, 240);
    const CamoRamp mr = camoRampFrom(m);
    CHECK(mr.count == kCamoRampMax);
    for (int i = 1; i < mr.count; ++i)
        CHECK(luminance(mr.tone[i - 1]) <= luminance(mr.tone[i]));
    // ...and a ladder that actually spans, rather than eight of the same tone.
    CHECK(luminance(mr.tone[mr.count - 1]) > luminance(mr.tone[0]) + 0.2f);
    bool carriesTheInk = false;
    for (int i = 0; i < mr.count; ++i) carriesTheInk |= (mr.tone[i] == m.ink);
    CHECK(carriesTheInk);                 // ...built around the colour it was given
    // Grey, not a hue: every tone on it is its own r=g=b within rounding, because the
    // ink was and both ends of the ladder (PAPER, INK) are neutral too.
    for (int i = 0; i < mr.count; ++i) {
        const int rr = r8(mr.tone[i]), gg = g8(mr.tone[i]), bb = b8(mr.tone[i]);
        CHECK(std::abs(rr - gg) <= 12 && std::abs(gg - bb) <= 12);
    }

    // An empty ramp is the no-op the draw path leans on, not a crash.
    CamoRamp none;
    CHECK(none.empty());
}

// The hybrid rule, from the other end: a source with real colours in it is worn in AS
// MANY OF THEM AS IT HAS, and the derived baseline only shows through at the values it
// never uses. So a well-drawn opponent is genuinely that opponent's palette, not an
// approximation of it.
void test_camo_rich_palette_is_mostly_the_real_thing() {
    // Ten tones spread across the value range — more than the ladder has rungs, and
    // deliberately not evenly spaced, which is what real pixel art looks like.
    uint16_t rgb[10];
    uint8_t alpha[10];
    for (int i = 0; i < 10; ++i) {
        rgb[i] = rgb565(static_cast<uint8_t>(24 * i + 8), static_cast<uint8_t>(24 * i + 8),
                        static_cast<uint8_t>(240 - 20 * i));
        alpha[i] = 255;
    }
    SpriteData rich{};
    rich.sheetW = 10; rich.h = 1; rich.frameW = 10; rich.frames = 1; rich.rows = 1;
    rich.rgb = rgb; rich.a = alpha;

    const CamoRamp r = camoRampFrom(rich);
    CHECK(r.count == kCamoRampMax);
    for (int i = 1; i < r.count; ++i)
        CHECK(luminance(r.tone[i - 1]) <= luminance(r.tone[i]));   // still a value scale

    int real = 0;
    for (int i = 0; i < r.count; ++i)
        for (int j = 0; j < 10; ++j)
            if (rgb[j] == r.tone[i]) { ++real; break; }
    // Most of the ladder is the source's own colours; the rest is the baseline showing
    // through at values the source simply does not have a colour for.
    CHECK(real >= r.count / 2);
}

// --- Gate: the level arrives, HOLDS, and releases only when told to ---
//
// The promise the whole effect rests on. A colour that says "this pet is channelling
// another line" has to stay put for as long as that is true — a level that drifted back
// on its own would be describing a swing that finished rather than a move the pet is
// still holding. So both ends are fixed points, and the only thing that moves the level
// is the answer to `worn` changing.
void test_camo_level_holds_and_releases() {
    uint8_t l = 0;
    int ticks = 0;
    while (l < 255 && ticks < 64) { l = camoAdvance(l, /*worn=*/true); ++ticks; }
    CHECK(l == 255);                      // it does fully arrive...
    CHECK(ticks > 1);                     // ...as a change you can see, not a snap
    for (int i = 0; i < 32; ++i) l = camoAdvance(l, /*worn=*/true);
    CHECK(l == 255);                      // ...and then it STAYS, however long it is held

    ticks = 0;
    while (l > 0 && ticks < 64) { l = camoAdvance(l, /*worn=*/false); ++ticks; }
    CHECK(l == 0);                        // released, it goes all the way back...
    CHECK(ticks > 1);
    for (int i = 0; i < 32; ++i) l = camoAdvance(l, /*worn=*/false);
    CHECK(l == 0);                        // ...and settles on the pet's own colours
}

// --- Gate: at rest the effect is exactly the ordinary draw ---
//
// progress 0 and an empty ramp are both "not camouflaged", and both must put the
// creature's OWN pixels on the canvas — otherwise every fighter not currently borrowing
// would be paying for this effect in colour accuracy.
void test_camo_zero_is_the_plain_draw() {
    Steps g;
    const CamoRamp r = camoRampFrom(g.s);

    Framebuffer plain(kActiveW, kActiveH), zero(kActiveW, kActiveH),
        empty(kActiveW, kActiveH);
    plain.clear(0); zero.clear(0); empty.clear(0);
    drawSpriteUpscaled(plain, g.s, 0, 0, 0, 1, 1);
    drawSpriteCamo(zero, g.s, 0, 0, 0, 1, 1, r, /*progress=*/0);
    drawSpriteCamo(empty, g.s, 0, 0, 0, 1, 1, CamoRamp{}, /*progress=*/255);
    for (int x = 0; x < 4; ++x) {
        CHECK(zero.get(x, 0) == plain.get(x, 0));
        CHECK(empty.get(x, 0) == plain.get(x, 0));
    }

    // Fully worn, every pixel is a tone from the ramp — the creature is entirely in
    // somebody else's colours rather than partly in its own.
    Framebuffer worn(kActiveW, kActiveH);
    worn.clear(0);
    const CamoRamp other = [] {
        CamoRamp c; c.count = 2;
        c.tone[0] = rgb565(10, 0, 0); c.tone[1] = rgb565(250, 0, 0); return c;
    }();
    drawSpriteCamo(worn, g.s, 0, 0, 0, 1, 1, other, /*progress=*/255);
    for (int x = 0; x < 4; ++x) {
        const Rgb565 c = worn.get(x, 0);
        CHECK(c == other.tone[0] || c == other.tone[1]);
    }
}

// --- Gate: a hit taken in borrowed colours flashes the CAMOUFLAGED pet ---
//
// The impact flash and the camouflage are both whole-body recolours, and drawn as
// alternatives the flash won every frame the pet was struck — which read exactly like
// getting hit cancelled the disguise. Composed, the hit is still legible (it has its own
// release gate) and the colours underneath it are still the borrowed ones.
void test_camo_flash_composes_over_it() {
    Steps g;
    const CamoRamp r = camoRampFrom(g.s);
    const CamoRamp other = [] {
        CamoRamp c; c.count = 2;
        c.tone[0] = rgb565(10, 0, 0); c.tone[1] = rgb565(250, 0, 0); return c;
    }();
    const Rgb565 white = rgb565(255, 255, 255);

    Framebuffer worn(kActiveW, kActiveH), lit(kActiveW, kActiveH),
        flashOnly(kActiveW, kActiveH);
    worn.clear(0); lit.clear(0); flashOnly.clear(0);
    drawSpriteCamo(worn, g.s, 0, 0, 0, 1, 1, other, /*level=*/255);
    drawSpriteCamo(lit, g.s, 0, 0, 0, 1, 1, other, /*level=*/255, white, /*flashAmt=*/200);
    drawSpriteFlash(flashOnly, g.s, 0, 0, 0, 1, 1, white, /*flashAmt=*/200);

    bool flashSeen = false, camoKept = false;
    for (int x = 0; x < 4; ++x) {
        if (lit.get(x, 0) != worn.get(x, 0)) flashSeen = true;        // the hit reads...
        if (lit.get(x, 0) != flashOnly.get(x, 0)) camoKept = true;    // ...over the camo
    }
    CHECK(flashSeen);
    CHECK(camoKept);

    // With no camouflage on, the same call is exactly the ordinary flash — the effect
    // costs a fighter that is not wearing anything nothing at all.
    Framebuffer bare(kActiveW, kActiveH);
    bare.clear(0);
    drawSpriteCamo(bare, g.s, 0, 0, 0, 1, 1, r, /*level=*/0, white, /*flashAmt=*/200);
    for (int x = 0; x < 4; ++x) CHECK(bare.get(x, 0) == flashOnly.get(x, 0));
}

// --- Gate: the colours hold on the beat the RIVAL is the one striking ---
//
// The bug this stands on. Derived from `hitBeat` — the most recent strike by EITHER
// fighter — the camouflage was taken off the pet the instant the rival hit back, which
// is the commonest thing that can happen next. The screen now reads a standing level, so
// this frame, in which the pet is mid-recoil from a hit it just took, is still a
// camouflaged pet.
void test_camo_holds_through_a_counter_strike() {
    ContentRegistry reg = ContentRegistry::embedded();
    const SpriteData* ps = reg.sprite("SPR_PET_PAYPUP");
    const SpriteData* es = reg.sprite("SPR_PET_PINGCUB");
    const CombatStage st = combatStage(ps, es);

    Combat c;
    {
        Combatant p = mkCombatant(reg, "P", 400, 1, {"quick_jab"});
        Combatant e = mkCombatant(reg, "E", 400, 20, {"quick_jab"});
        c.begin(p, e, Combat::Stakes::Safe, 7);
        for (int i = 0; i < 40; ++i) {
            c.step();
            if (c.lastWasStrike() && !c.lastByPlayer()) break;
        }
        CHECK(c.lastWasStrike() && !c.lastByPlayer());   // the rival owns the strike clock
    }

    auto shot = [&](uint8_t level, Framebuffer& fb) {
        CombatCamo camo;
        camo.level = level;
        camo.ramp = camoRampFrom(*es);   // the caller resolves the palette (camoTarget)
        drawCombat(fb, c, ps, es, 0, /*animBeat=*/4, /*hitBeat=*/1, 0, CombatSides{},
                   CombatOutro{}, RivalPrizes{}, camo);
    };
    Framebuffer bare(kActiveW, kActiveH), held(kActiveW, kActiveH);
    shot(/*level=*/0, bare);
    shot(/*level=*/255, held);

    const int x0 = std::max(0, st.localX), x1 = std::min(kActiveW, st.localX + st.localW);
    CHECK(regionDiffers(bare, held, x0, 81, x1, 165));
}

// --- Gate: the state the level is eased toward survives the rival's whole turn ---
//
// The input to everything above, asserted in the fight rather than at the draw. What the
// pet is wearing is a reading of ITS OWN last cast (`Combatant::lastMoveIdx`,
// `WildPool::lastRolled`), and both are per-fighter and rewritten only when that fighter
// acts — so a borrowed move is still what the pet is holding while the rival takes its
// turn, hits it, and hits it again. An effect derived from anything the rival's turn
// moves comes off the pet at the commonest thing that can happen next.
void test_borrowed_colours_outlive_the_rivals_turn() {
    ContentRegistry reg = ContentRegistry::embedded();
    const CreatureDef* meta = reg.creature("cuttlefork");
    CHECK(meta != nullptr && meta->line != nullptr);

    // A wildcard pool is generic-plus-two-lines, so which band a cast lands in is a roll.
    // Walking a few seeds gets both outcomes: a BORROWED row (the case under test) and a
    // generic one, which is the pet back in its own colours.
    bool sawBorrowed = false, sawOwn = false;
    int rivalTurnsWhileWorn = 0;
    for (uint32_t seed = 1; seed <= 8; ++seed) {
        Combatant p = mkCombatant(reg, "P", 4000, 5, {"instruction_swap"});
        p.creature = meta;
        p.stage = Stage::Script;
        buildWildPools(reg, p, p.stage);
        CHECK(p.polymorphic);                        // the row really is a wildcard
        Combatant e = mkCombatant(reg, "E", 4000, 25, {"quick_jab"});
        Combat c;
        c.begin(p, e, Combat::Stakes::Safe, seed);

        // Walk the fight. After each of the PET's own casts, whose colours it is in is
        // that cast's answer; after everything else — every blow the rival lands in
        // between — it has to still be the same answer, because none of that is the
        // pet's cast.
        CamoTarget worn;
        for (int i = 0; i < 120 && c.outcome() == Combat::Outcome::Ongoing; ++i) {
            c.step();
            if (c.lastByPlayer()) {
                worn = camoTarget(c.player(), c.enemy());
                (worn.source != CamoTarget::Source::Own ? sawBorrowed : sawOwn) = true;
            } else if (worn.source != CamoTarget::Source::Own) {
                ++rivalTurnsWhileWorn;
                CHECK(camoTarget(c.player(), c.enemy()) == worn);
            }
        }
    }
    CHECK(sawBorrowed);            // a wildcard does reach another line's pool...
    CHECK(sawOwn);                 // ...and does also come back generic, its own colours
    CHECK(rivalTurnsWhileWorn > 0);  // and the rival did act while the pet was wearing them
}

// --- Gate: a borrowed cast is worn even when the rival is nothing to do with it ---
//
// The single-player case, and the one the whole effect lives or dies on: a wild malbeast
// belongs to no line and fields only generic rows, so a rule that asked the fighter
// opposite to be carrying the rolled move left the pet in its own colours through every
// fight the game actually offers. What a borrowed cast names then is the LINE it came
// out of, which is a fact about the cast and needs nothing from the other seat.
void test_borrowed_line_is_worn_against_a_rival_that_has_nothing_to_do_with_it() {
    ContentRegistry reg = ContentRegistry::embedded();
    const CreatureDef* meta = reg.creature("cuttlefork");
    CHECK(meta != nullptr);

    Combatant p = mkCombatant(reg, "P", 4000, 5, {"instruction_swap"});
    p.creature = meta;
    p.stage = Stage::Script;
    buildWildPools(reg, p, p.stage);
    const MoveDef* borrowed = nullptr;
    int slot = -1;
    for (size_t i = 0; i < p.wildPools.size() && !borrowed; ++i)
        for (const MoveDef* row : p.wildPools[i].rows)
            if (row && row->line && std::strcmp(row->line, meta->line) != 0) {
                borrowed = row;
                slot = static_cast<int>(i);
                break;
            }
    CHECK(borrowed != nullptr && slot >= 0);
    p.wildPools[slot].lastRolled = borrowed;
    p.lastMoveIdx = slot;

    // A real wild: built from a spec, so it carries no creature and therefore no line,
    // and its kit is the generic spine.
    Combatant wild = makeEnemyCombatant(reg, wildMalbeast(1, 0));
    CHECK(wild.creature == nullptr);
    const CamoTarget t = camoTarget(p, wild);
    CHECK(t.source == CamoTarget::Source::Line);
    CHECK(t.line != nullptr && std::strcmp(t.line, borrowed->line) == 0);
}

// --- Gate: copying the fighter opposite outranks wearing the line ------------
//
// Two ways a cast can be somebody else's, and the pet takes the more specific one. A move
// sitting in the rival's own kit — or belonging to the rival's line — makes the pet a copy
// of THAT creature, down to whatever accent it alone carries; a move from a line the
// rival has nothing to do with makes it the line. The rank matters because both are true
// at once whenever the rival is on the line the roll came from.
void test_camo_copies_the_rival_before_the_line() {
    ContentRegistry reg = ContentRegistry::embedded();
    const CreatureDef* meta = reg.creature("cuttlefork");
    CHECK(meta != nullptr);

    Combatant p = mkCombatant(reg, "P", 4000, 5, {"instruction_swap"});
    p.creature = meta;
    p.stage = Stage::Script;
    buildWildPools(reg, p, p.stage);
    const MoveDef* borrowed = nullptr;
    const MoveDef* generic = nullptr;
    int slot = -1;
    for (size_t i = 0; i < p.wildPools.size(); ++i)
        for (const MoveDef* row : p.wildPools[i].rows) {
            if (row && row->line && std::strcmp(row->line, meta->line) != 0 && !borrowed) {
                borrowed = row;
                slot = static_cast<int>(i);
            }
            // Any generic row the stranger below is not itself holding — the innate jab
            // is in every kit in the game, so it cannot separate the two cases.
            if (row && !row->line && !generic && std::strcmp(row->id, "quick_jab") != 0)
                generic = row;
        }
    CHECK(borrowed != nullptr && generic != nullptr && slot >= 0);

    // A creature ON the line the cast came from: the pet is copying that fighter, whether
    // or not this particular one has the row equipped.
    const CreatureDef* owner = nullptr;
    for (const CreatureDef* c : reg.allCreatures())
        if (c->line && std::strcmp(c->line, borrowed->line) == 0) { owner = c; break; }
    CHECK(owner != nullptr);
    Combatant lineMate = mkCombatant(reg, "L", 4000, 25, {"quick_jab"});
    lineMate.creature = owner;
    p.wildPools[slot].lastRolled = borrowed;
    p.lastMoveIdx = slot;
    CHECK(camoTarget(p, lineMate).source == CamoTarget::Source::Rival);

    // A lineless rival that has the row in its kit is copied for the same reason: the
    // name the pet is wearing is one the stats page shows on the other side.
    Combatant carrier = mkCombatant(reg, "C", 4000, 25, {"quick_jab"});
    carrier.moves.push_back(borrowed);
    CHECK(camoTarget(p, carrier).source == CamoTarget::Source::Rival);

    // A GENERIC roll is nobody's line, so only the rival's kit can claim it — which is
    // what puts the pet in a wild malbeast's colours for a move they both know.
    p.wildPools[slot].lastRolled = generic;
    Combatant stranger = mkCombatant(reg, "S", 4000, 25, {"quick_jab"});
    CHECK(camoTarget(p, stranger).source == CamoTarget::Source::Own);
    Combatant sharer = mkCombatant(reg, "H", 4000, 25, {"quick_jab"});
    sharer.moves.push_back(generic);
    CHECK(camoTarget(p, sharer).source == CamoTarget::Source::Rival);
}
