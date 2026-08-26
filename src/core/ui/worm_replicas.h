// worm_replicas.h — the two glyphs the Worm line's copies are drawn with, and how
// one of them is seated on a shelf.
//
// The line shares ONE pair of glyphs (SPR_WORM_REPLICA_ATTACK/_DEFEND) across every
// worm, rather than every worm sprite carrying a shrunken self — see
// assets/CREATURE_VISUAL_RULES.md for why. Each is a 6-frame strip in idle/attack/
// death pairs; the base frame says which pair, and `+ (beat & 1)` flips within it.
//
// This sits in its own header because two screens draw copies now and they must
// agree: combat_screen.cpp seats the fighting board between a worm and its enemy,
// and the idle habitat (game_render.cpp) walks a few ambient ones beside their
// parent. A copy that looked like one thing in a fight and another at home would
// stop reading as the same creature.
#pragma once

#include "core/render/canvas.h"   // kScaleNum, kScaleDen — the habitat's sprite scale
#include "core/render/sprite.h"   // SpriteData, drawSpriteUpscaled

namespace mal {

class Framebuffer;

constexpr int kReplicaIdleFrame = 0;    // + (beat & 1) for the squiggle
constexpr int kReplicaAttackFrame = 2;  // + (beat & 1) for the chomp
constexpr int kReplicaDeathFrame = 4;   // + (beat & 1) for the dissolve

// One slot's width on the shelf: the glyph plus a little air, so neighbours read as
// separate copies rather than one crowd.
constexpr int kReplicaSlotW = 30;

// Seat one glyph. Bottom-anchored on the shelf like the fighters, so the replicas
// stand on the same ground line the parent does.
//
// The scale defaults to the habitat's, which is the only one the idle screen has. The
// fight passes its own: the combat stage picks a SHOT per pairing (CombatStage,
// ui/combat_screen.h), and a copy drawn at the habitat's scale on a wide-shot stage
// would stand taller than the worm that made it.
inline void drawReplica(Framebuffer& fb, const SpriteData& s, int frame, int cx,
                        int shelfY, int num = kScaleNum, int den = kScaleDen) {
    const int w = s.frameW * num / den;
    const int h = s.h * num / den;
    drawSpriteUpscaled(fb, s, frame, cx - w / 2, shelfY - h, num, den);
}

}  // namespace mal
