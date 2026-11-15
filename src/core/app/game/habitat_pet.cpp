#include "core/app/game/home_habitat.h"

#include "core/render/framebuffer.h"
#include "core/render/camo.h"
#include "core/render/canvas.h"
#include "core/render/cloak.h"
#include "core/render/glitch.h"
#include "core/render/palette.h"
#include "core/render/rain.h"
#include "core/render/sprite.h"

// habitat_pet.cpp: the resting pet and what it is doing, back to front. The habitat
// screen draws the backdrop under it and its status and carousel over it
// (Game::drawHabitat).

namespace mal {

void HomeHabitat::draw(Framebuffer& fb, const HabitatBeat& pet, const SpriteData* sprite,
                       int beat) const {
    // Drawn before the pet so a fork crawls out from behind its body.
    drawReplicas(fb, pet, kActiveW / 2 + logicalToActive(wander_.offsetX()), beat);
    if (sprite && pet.creature) drawPet(fb, *pet.creature, *sprite, beat);
    drawFeedingMail(fb, beat);
}

void HomeHabitat::drawPet(Framebuffer& fb, const CreatureDef& creature, const SpriteData& pet,
                          int beat) const {
    // An authored clip plays its full row in order; otherwise fall back to the
    // breathe/blink heuristic on row 0. A creature that authored a "walk" takes it
    // for exactly the beats the wander below is actually moving the anchor, so the
    // legs and the drift are one motion rather than two that happen to overlap;
    // one that authored none simply keeps breathing while it slides, which is what
    // every single-row sheet does.
    const AnimClip* clip = nullptr;
    if (feeding_.biting() || lure_.biting()) clip = creature.clip("attack");
    if (!clip && wander_.travelling()) clip = creature.clip("walk");
    if (!clip) clip = creature.clip("idle");
    PetPose pose;
    pose.sprite = &pet;
    pose.row = clip ? clip->row : 0;
    pose.frame = clip ? clip->frameAt(beat) : idleFrame(pet, beat);
    // Two motions, and the creature is the only thing on this screen that takes
    // either. The bob is the pose's own 2px lift on alternate beats, which
    // animates a single-frame creature. A swimmer skips it, because a bob on top
    // of a continuous drift reads as jitter. The WANDER moves the whole anchor
    // around the living box, in logical px (core/model/idle_wander.h): sideways
    // for every mover, and off the shelf for the ones that don't need the floor.
    const int bob = (IdleWander::bobs(creature.locomotion) && (beat & 1) == 0) ? 2 : 0;
    const int petW = pet.frameW * kScaleNum / kScaleDen;
    const int petH = pet.h * kScaleNum / kScaleDen;
    pose.x = (kActiveW - petW) / 2 + logicalToActive(wander_.offsetX());
    pose.y = kLivingBottom - petH - bob - logicalToActive(wander_.offsetY());
    // Turned to the way it is actually going. A sheet with no declared facing is
    // never mirrored, so this only turns the creatures the wander was walking
    // backwards: the ones drawn in profile instead of §2's turned-to-the-viewer
    // pose (assets/CREATURE_VISUAL_RULES.md). The habitat has no seat and no
    // opponent, so unlike the combat stage the direction comes off the trip
    // itself.
    pose.mirror = spriteMirrorToFace(pet, wander_.headingRight());
    const int frame = pose.frame, row = pose.row, petX = pose.x, petY = pose.y;
    const bool mirror = pose.mirror;
    // Resting colour: a line that wears borrowed colours spends a few seconds now
    // and then in another family's palette (core/model/idle_camo.h). The level and
    // the art are the step's; this ranks that art into the ladder the creature is
    // repainted through, exactly as the combat screen does. Every other pet, and
    // this one between drifts, takes the plain blit and pays nothing.
    const CamoRamp drift = camoWorn_ ? camoRampFrom(*camoWorn_) : CamoRamp{};
    if (disguise_.showsProp())
        drawDisguiseProp(fb, petX + petW / 2, beat);
    else if (harvest_.stage() == HarvestStage::Hold)
        drawSpriteRain(fb, pet, frame, petX, petY, kScaleNum, kScaleDen,
                       palColor(Pal::PAPER), palColor(Pal::DECRYPTION_GREEN), beat, row,
                       mirror);
    else if (vanish_.active())
        drawSpriteCloak(fb, pet, frame, petX, petY, kScaleNum, kScaleDen, vanish_.amount(),
                        palColor(Pal::INK), beat, row, mirror);
    else if (mimic_.active() && mimicWorn_)
        drawMimic(fb, pose, beat);
    else if (disguise_.torn())
        drawSpriteGlitch(fb, pet, frame, petX, petY, kScaleNum, kScaleDen,
                         palColor(Pal::INK), palColor(Pal::HOT),
                         static_cast<uint8_t>(beat * 32), row, mirror);
    else if (camo_.level() > 0 && !drift.empty())
        drawSpriteCamo(fb, pet, frame, petX, petY, kScaleNum, kScaleDen, drift,
                       camo_.level(), /*flashColor=*/0, /*flashAmt=*/0, row,
                       /*from=*/nullptr, mirror);
    else
        drawSpriteUpscaled(fb, pet, frame, petX, petY, kScaleNum, kScaleDen, row, mirror);
    drawHarvestStreams(fb, petX + petW / 2, petY + petH / 2, beat);
    drawLure(fb, petX + petW / 2, petY + petH / 4, beat);
}

}  // namespace mal
