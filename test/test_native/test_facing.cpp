// test_facing.cpp — native gates for how a fighter is POSED on the combat stage: which
// way its drawing is turned (the sheet's declared Facing, core/render/sprite.h, and the
// mirror every blitter takes off it), and when it wears its authored flinch rather than
// its idle (hurtPoseEarned, core/ui/combat_screen.h). Both answer the same question from
// opposite ends — what a fighter looks like it is doing on any given beat.
//
// One slice of the native suite; see test_gates.h for the shared fixtures and
// test_main.cpp for the roster that runs these.
#include "test_gates.h"

#include "core/render/absorb.h"
#include "core/render/camo.h"
#include "core/render/shred.h"
#include "core/model/combat.h"
#include "core/render/sprite.h"
#include "core/ui/combat_screen.h"
#include "generated/assets.h"

namespace {

// An asymmetric 4x2 cell: every pixel a different colour, and the left half opaque
// against a transparent right. Asymmetry is the whole point — a symmetric fixture
// cannot tell a mirror from a no-op, which is exactly the bug these gates exist to
// catch.
struct Lopsided {
    uint16_t rgb[8];
    uint8_t a[8] = {255, 255, 0, 0, 255, 255, 0, 0};
    SpriteData s{};
    Lopsided() {
        for (int i = 0; i < 8; ++i) rgb[i] = rgb565(10 + i * 30, 20, 200 - i * 20);
        s.sheetW = 4; s.h = 2; s.frameW = 4; s.frames = 1; s.rows = 1;
        s.contentX0 = 0; s.contentX1 = 2;   // drawn band is the left half of the cell
        s.rgb = rgb; s.a = a;
    }
};

// Is `b` the horizontal mirror of `a` across the band [x0, x1)? The two draws are made
// at the same origin, so a mirrored blit lands the far column where the near one was.
bool mirrorsAcross(const Framebuffer& a, const Framebuffer& b, int x0, int y0,
                   int x1, int y1) {
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            if (a.get(x, y) != b.get(x1 - 1 - (x - x0), y)) return false;
    return true;
}

}  // namespace

// --- Gate: a mirrored blit is the plain blit, reversed ---
//
// The one property everything else here leans on. Both draws walk the same destination
// and differ only in which source column they read (spriteSrcX), so the result has to
// be an exact reflection — not merely "different", which a sampling bug would also be.
void test_facing_mirror_reverses_the_draw() {
    Lopsided g;
    Framebuffer plain(kActiveW, kActiveH), flipped(kActiveW, kActiveH);
    plain.clear(0); flipped.clear(0);
    drawSpriteUpscaled(plain, g.s, 0, 0, 0, 1, 1);
    drawSpriteUpscaled(flipped, g.s, 0, 0, 0, 1, 1, /*row=*/0, /*mirror=*/true);
    CHECK(!fbEqual(plain, flipped));                       // the fixture IS asymmetric
    CHECK(mirrorsAcross(plain, flipped, 0, 0, g.s.frameW, g.s.h));

    // At the creature scale too: the upscale samples source columns through the same
    // seam, so a mirror that only worked 1:1 would be a mirror that never ran in a
    // fight. ×1.75 is the hybrid model's one creature ratio (CONTRIBUTING.md).
    Framebuffer bigPlain(kActiveW, kActiveH), bigFlipped(kActiveW, kActiveH);
    bigPlain.clear(0); bigFlipped.clear(0);
    drawSpriteUpscaled(bigPlain, g.s, 0, 0, 0, 7, 4);
    drawSpriteUpscaled(bigFlipped, g.s, 0, 0, 0, 7, 4, /*row=*/0, /*mirror=*/true);
    CHECK(!fbEqual(bigPlain, bigFlipped));
}

// --- Gate: the flash and the camouflage turn with the creature ---
//
// Both are per-pixel passes over the same walk, and both had to be threaded separately.
// A fighter that turned to face its opponent but flashed or camouflaged in its old
// orientation would come apart the instant it was hit, which is the beat it is most
// looked at.
void test_facing_mirror_reaches_every_creature_blit() {
    Lopsided g;
    const Rgb565 white = rgb565(255, 255, 255);

    Framebuffer flashPlain(kActiveW, kActiveH), flashFlipped(kActiveW, kActiveH);
    flashPlain.clear(0); flashFlipped.clear(0);
    drawSpriteFlash(flashPlain, g.s, 0, 0, 0, 1, 1, white, 120);
    drawSpriteFlash(flashFlipped, g.s, 0, 0, 0, 1, 1, white, 120, /*row=*/0,
                    /*mirror=*/true);
    CHECK(mirrorsAcross(flashPlain, flashFlipped, 0, 0, g.s.frameW, g.s.h));

    const CamoRamp ramp = camoRampFrom(g.s);
    Framebuffer camoPlain(kActiveW, kActiveH), camoFlipped(kActiveW, kActiveH);
    camoPlain.clear(0); camoFlipped.clear(0);
    drawSpriteCamo(camoPlain, g.s, 0, 0, 0, 1, 1, ramp, 255);
    drawSpriteCamo(camoFlipped, g.s, 0, 0, 0, 1, 1, ramp, 255, /*flashColor=*/0,
                   /*flashAmt=*/0, /*row=*/0, /*from=*/nullptr, /*mirror=*/true);
    // The camo grain is hashed on the SOURCE pixel, so it mirrors WITH the creature
    // rather than staying put underneath it — which is what makes this an exact
    // reflection and not merely a different speckle.
    CHECK(mirrorsAcross(camoPlain, camoFlipped, 0, 0, g.s.frameW, g.s.h));

    // The outro pair. A beaten rival keeps the turn it fought in, so these carry the
    // flag too; asserted as "the mirror changes the draw" rather than as a reflection,
    // since both scatter their pixels by destination and neither is a plain blit.
    Framebuffer shredPlain(kActiveW, kActiveH), shredFlipped(kActiveW, kActiveH);
    shredPlain.clear(0); shredFlipped.clear(0);
    drawShred(shredPlain, g.s, 0, 0, 0, 1, 1, white, /*progress=*/0);
    drawShred(shredFlipped, g.s, 0, 0, 0, 1, 1, white, /*progress=*/0, /*row=*/0,
              /*mirror=*/true);
    CHECK(!fbEqual(shredPlain, shredFlipped));

    Framebuffer absPlain(kActiveW, kActiveH), absFlipped(kActiveW, kActiveH);
    absPlain.clear(0); absFlipped.clear(0);
    drawAbsorb(absPlain, g.s, 0, 0, 0, 1, 1, 20, 1, white, /*progress=*/0, /*bite=*/255);
    drawAbsorb(absFlipped, g.s, 0, 0, 0, 1, 1, 20, 1, white, /*progress=*/0,
               /*bite=*/255, /*row=*/0, /*mirror=*/true);
    CHECK(!fbEqual(absPlain, absFlipped));
}

// --- Gate: a mirrored drawing's seat is measured from the other cell edge ---
//
// Seating is by CONTENT, not by cell (see spriteContentX0), and a sprite padded to one
// side of its cell moves that padding's width when it turns. Reading the un-mirrored
// span for a mirrored draw is therefore not a rounding error — it stands the creature
// visibly off its own seat.
void test_facing_content_span_follows_the_mirror() {
    Lopsided g;                                   // drawn band [0, 2) of a 4-wide cell
    CHECK(spriteContentX0(g.s) == 0);
    CHECK(spriteContentX1(g.s) == 2);
    CHECK(spriteContentX0(g.s, /*mirror=*/true) == 2);
    CHECK(spriteContentX1(g.s, /*mirror=*/true) == 4);
    // The WIDTH is mirror-invariant, which is why combatStage's seatWidth needs no flag.
    CHECK(spriteContentX1(g.s, true) - spriteContentX0(g.s, true) ==
          spriteContentX1(g.s) - spriteContentX0(g.s));

    // A sheet that never had its span measured answers with the whole frame either way,
    // so a zero-initialized placeholder cannot be seated somewhere nonsensical.
    SpriteData bare{};
    bare.frameW = 6; bare.frames = 1;
    CHECK(spriteContentX0(bare, true) == 0);
    CHECK(spriteContentX1(bare, true) == 6);
}

// --- Gate: only a sheet with a declared side is ever turned ---
//
// Facing::None is the floor, and it has to be inert: most of the roster is drawn in the
// three-quarter turned-to-the-viewer pose assets/CREATURE_VISUAL_RULES.md §2 asks for,
// and every icon and panel shares the default. Mirroring one of those would flip a
// reading direction to no purpose.
void test_facing_undeclared_is_never_mirrored() {
    SpriteData s{};
    s.facing = Facing::None;
    CHECK(!spriteMirrorToFace(s, /*faceRight=*/true));
    CHECK(!spriteMirrorToFace(s, /*faceRight=*/false));

    s.facing = Facing::Right;
    CHECK(!spriteMirrorToFace(s, true));    // already looking that way
    CHECK(spriteMirrorToFace(s, false));

    s.facing = Facing::Left;
    CHECK(spriteMirrorToFace(s, true));
    CHECK(!spriteMirrorToFace(s, false));
}

// --- Gate: the roster's declared facings survive a regeneration ---
//
// FACING lives in tools/gen_assets.py, so it is one table away from being silently
// dropped — and a dropped table is invisible: every sheet still renders, and half the
// roster quietly goes back to showing its opponent its tail. These four are the corners
// of the table: a pet drawn each way, a wild, and one of the many with no side at all.
void test_facing_roster_declarations_hold() {
    // The Metamorphic Process form is drawn head-left, and it holds the LEFT seat, so it
    // is the row that has to mirror for the local pet to look into the fight at all.
    CHECK(ASSET_SPR_PET_CUTTLEFORK.facing == Facing::Left);
    CHECK(spriteMirrorToFace(ASSET_SPR_PET_CUTTLEFORK, /*faceRight=*/true));

    // Drawn head-right, so the same seat costs it nothing.
    CHECK(ASSET_SPR_PET_BAITRACUDA.facing == Facing::Right);
    CHECK(!spriteMirrorToFace(ASSET_SPR_PET_BAITRACUDA, /*faceRight=*/true));

    // A wild always takes the RIGHT seat and the whole malbeast line is drawn head-left,
    // so the line already looks into the fight and mirrors nowhere.
    CHECK(ASSET_SPR_MALBEAST_GLITCHHOG.facing == Facing::Left);
    CHECK(!spriteMirrorToFace(ASSET_SPR_MALBEAST_GLITCHHOG, /*faceRight=*/false));

    // The gold standard stands square to the viewer and has no side to turn.
    CHECK(ASSET_SPR_PET_PAYPUP.facing == Facing::None);
    CHECK(!spriteMirrorToFace(ASSET_SPR_PET_PAYPUP, true));
}

// --- Gate: the flinch is reserved for hits that earned it ---
//
// The reason this file's other gates matter: a fight the player can read. Fighters
// alternate, so "was damaged" is true of somebody on nearly every resolved turn — the
// pose has to answer a narrower question than that or it is on screen permanently.
void test_hurt_pose_is_reserved_for_real_hits() {
    Combatant c{};
    c.maxHealth = 100;

    // A chip does not move it; a real bite does. The bar is the target's own pool.
    CHECK(!hurtPoseEarned(c, 1));
    CHECK(!hurtPoseEarned(c, kHurtPosePctOfMax - 1));
    CHECK(hurtPoseEarned(c, kHurtPosePctOfMax));
    CHECK(hurtPoseEarned(c, 90));

    // Same PROPORTION, four times the pool: a Daemon does not flinch at a hit that would
    // have doubled a Process form over, and does flinch at one that takes the same share.
    Combatant big{};
    big.maxHealth = 400;
    CHECK(!hurtPoseEarned(big, 25));
    CHECK(hurtPoseEarned(big, 80));

    // Locked outranks the threshold: being hit while unable to act is the case the pose
    // describes best, whatever the hit was worth.
    c.lockedTurnsLeft = 2;
    CHECK(hurtPoseEarned(c, 1));

    // A fighter with no pool at all (a placeholder, a dummy built without one) never
    // flinches rather than dividing by nothing.
    Combatant empty{};
    CHECK(!hurtPoseEarned(empty, 50));
}
