// test_scene.cpp — what a gate can hold about an engine-drawn BACKDROP.
//
// Neither tier can see composition. Whether a place READS is a question for
// tools/dump_frame.cpp's `scene:<name>` and a pair of eyes, and a scene that passes
// everything here and looks like a grey slab has still failed. What is checkable is the
// three properties the whole design rests on, and each of them is exactly the kind of
// thing a person reviewing a picture does not notice:
//
//   * PORTABILITY. A backdrop serves screens whose floors are 44 rows apart, so every
//     scene is rendered at BOTH and has to compose at both — nothing off the canvas,
//     nothing collapsed onto the ground.
//   * CONTRAST. A screen's own rows are drawn at `ink` straight over this, so the
//     brightest thing a scene paints has to stay a long way under it.
//   * THE ANCHOR RAIL. `accent` means focus and a status hue means a state; a backdrop
//     wearing either is lying about the screen. sceneTint refuses, and this is what
//     says it still does.
#include "test_gates.h"

#include "core/content/areas/area_defs.h"
#include "core/content/content_homes.h"
#include "core/content/creatures/creature_lines.h"
#include "core/render/scene.h"
#include "core/render/scenes.h"

namespace {

// The two floors in play, and the whole reason a scene is composed against a ground
// rather than against the canvas: fighters stand on one and a resting pet on the other.
constexpr int kFloors[] = {kCombatSpriteShelf, kLivingBottom};

// How much brighter than the wide ceiling (sceneCeiling, which is the rule itself
// rather than a restatement of it) a scene is allowed in total. Accents — a lit window,
// a via, a rack lamp — are a few pixels each and there are a handful per scene; the
// budget is what tells those from a scene that has quietly painted a bright band
// across itself.
constexpr int kAccentBudget = 160;

// How much darker than `ink` the brightest scene pixel has to stay. Text drawn over a
// backdrop is the case this whole ramp exists to survive, and a delta is the checkable
// half of it — the rest is the grayscale contact sheet.
constexpr float kInkDelta = 0.30f;

// Every scene the catalogue holds, which is what stops a new one from being added
// without one. SceneId::None draws nothing and is skipped by the loop itself.
int sceneCount() { return static_cast<int>(SceneId::Count); }

}  // namespace

// Every authored place renders at both floors, paints something, and stays under the
// tone ceiling except for a small budget of accents.
void test_scene_composes_at_both_floors() {
    const float inkLum = luminance(palColor(Pal::INK));
    const float wideLum = luminance(sceneCeiling());
    for (int i = 1; i < sceneCount(); ++i) {
        const SceneId id = static_cast<SceneId>(i);
        CHECK(sceneFor(id) != nullptr);
        CHECK(sceneName(id)[0] != '\0');
        CHECK(sceneByName(sceneName(id)) == id);
        for (int floorY : kFloors) {
            Framebuffer fb(kActiveW, kActiveH);
            CHECK(drawScene(fb, id, /*beat=*/3, sceneGround(floorY)));

            int accents = 0;
            float brightest = 0.0f;
            // A scene that composed itself off the top of the canvas is a scene whose
            // sky band came out empty, so the band above the horizon is counted
            // separately from the whole.
            bool skyPainted = false, groundPainted = false;
            const Rgb565 paper = palColor(Pal::PAPER);
            const int horizonY = sceneGround(floorY).horizonY;
            for (int y = 0; y < kActiveH; ++y) {
                for (int x = 0; x < kActiveW; ++x) {
                    const Rgb565 c = fb.get(x, y);
                    if (c != paper) (y < horizonY ? skyPainted : groundPainted) = true;
                    const float l = luminance(c);
                    if (l > brightest) brightest = l;
                    if (l > wideLum) ++accents;
                }
            }
            CHECK(groundPainted);
            CHECK(skyPainted);
            CHECK(accents <= kAccentBudget);
            CHECK(inkLum - brightest >= kInkDelta);
        }
    }
}

// `ink` text drawn over a scene's sky keeps its separation. Drawn for real rather than
// reasoned about, because the question is whether the GLYPHS land brighter than what
// they land on, and a glyph is a handful of pixels in a row of mostly background.
void test_scene_keeps_ink_legible() {
    for (int i = 1; i < sceneCount(); ++i) {
        const SceneId id = static_cast<SceneId>(i);
        for (int floorY : kFloors) {
            Framebuffer fb(kActiveW, kActiveH);
            drawScene(fb, id, /*beat=*/0, sceneGround(floorY));
            const int rowY = sceneGround(floorY).horizonY / 2;
            // What the sky under the row is, before anything is written on it.
            float under = 0.0f;
            for (int x = 0; x < kActiveW; ++x)
                for (int y = rowY; y < rowY + kFontH; ++y) {
                    const float l = luminance(fb.get(x, y));
                    if (l > under) under = l;
                }
            drawText(fb, 8, rowY, "ROUND 3/3  CHAMPION", palColor(Pal::INK));
            float lit = 0.0f;
            for (int x = 8; x < kActiveW - 8; ++x)
                for (int y = rowY; y < rowY + kFontH; ++y) {
                    const float l = luminance(fb.get(x, y));
                    if (l > lit) lit = l;
                }
            CHECK(lit - under >= kInkDelta);
        }
    }
}

// The anchor rail: a scene cannot borrow a colour that already means something. Asking
// for one hands back the plain value ramp instead, so the rule holds in the build and
// not only in review.
void test_scene_tint_refuses_interface_colours() {
    const Pal kForbidden[] = {Pal::ACCENT, Pal::CALM, Pal::WARN, Pal::HOT};
    for (Pal p : kForbidden) CHECK(sceneTint(150, p) == sceneTone(150));
    // ...and the fragmentation pair, which no interface state has claimed, is let
    // through — otherwise the rail would be indistinguishable from a broken function.
    CHECK(sceneTint(150, Pal::FRAG_HI) != sceneTone(150));
    CHECK(sceneTint(0, Pal::FRAG_HI) == palColor(Pal::PAPER));
}

// Every area either names a place that exists or says it has none. This is what makes
// AreaDef::scene a fact the build checks rather than a field somebody remembers to set.
void test_every_area_names_a_real_scene() {
    for (int i = 0; i < kAreaCount; ++i) {
        const AreaDef& a = area(i);
        const int id = static_cast<int>(a.scene);
        CHECK(id >= 0 && id < sceneCount());
        if (a.scene != SceneId::None) CHECK(sceneFor(a.scene) != nullptr);
    }
    // The ladder's first area is the one every operator walks, so it is the one place
    // where "not authored yet" would actually be noticed.
    CHECK(area(0).scene != SceneId::None);
}

// Every creature on the roster is somewhere. The chain in content/content_homes.h ends
// in a locomotion, and every Locomotion answers — so this is a total function and the
// gate is what says it stayed one when a line or a mover was added.
void test_every_creature_has_a_home() {
    int homed = 0;
    for (const CreatureLine& l : kCreatureLines)
        for (int i = 0; i < l.count; ++i) {
            const SceneId s = sceneForCreature(l.rows[i]);
            CHECK(s != SceneId::None);
            CHECK(sceneFor(s) != nullptr);
            ++homed;
        }
    CHECK(homed > 0);
    // The two facts the chain is built on, stated directly: a line with a place of its
    // own overrides how its creatures move, and a line without one falls through to it.
    // Phishing walks and swims in equal measure, which is exactly why it is the override
    // that earns its keep.
    for (const LineHome& h : kLineHomes) CHECK(sceneFor(h.scene) != nullptr);
    CHECK(sceneForLocomotion(Locomotion::Swim) != sceneForLocomotion(Locomotion::Fly));
    CHECK(sceneForLocomotion(Locomotion::Ground) != sceneForLocomotion(Locomotion::Walk));
}

// A screen's own choice of place: the habitat stands the pet where it lives, and a
// fight with no area behind it is fought in the same spot. Both are what makes the
// backdrop reachable at all — a catalogue nothing draws from is a catalogue.
void test_screens_choose_a_place() {
    Game g{StartMode::Hatched, "cuttlefork"};
    CHECK(g.habitatScene() == sceneForCreature(*g.pet()));
    CHECK(g.stageScene() == g.habitatScene());   // no walk armed: fought where it lives

    // ...and the habitat actually paints it, rather than the plain field it used to.
    Framebuffer fb(kActiveW, kActiveH);
    g.tick(0);
    g.render(fb);
    Framebuffer bare(kActiveW, kActiveH);
    drawScene(bare, g.habitatScene(), 0, sceneGround(kLivingBottom));
    // A row of the living band that no chrome and no creature reaches: the far left,
    // just under the top track, is backdrop and nothing else.
    CHECK(!regionDiffers(fb, bare, 0, kLivingTop + 2, 6, kLivingTop + 18));
}
