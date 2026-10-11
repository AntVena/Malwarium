// glitch.h — FX_GLITCH: the wild-malbeast corruption shader.
//
// A malbeast's sprite is a cheap one-ink blocky silhouette (tools/gen_malbeast_art.py):
// no colour, no eyes, no animation of its own. ALL of the menace — the thing that makes a
// wild read as hostile corrupted software rather than as a pet — is this pass, applied over
// that silhouette at draw time. Keeping it here rather than in the pixels is what lets every
// malbeast share one scary, animated treatment while each costs about what a still costs:
// the per-creature half is a dozen lines of recipe, the expensive half is this one function.
//
// The corruption, in three tells, all quantised to the sprite's own 4px nibble so the glitch
// is as low-def as the body (a smooth tear would read as a different, cleaner renderer):
//   1. SLICE TEARING   — horizontal bands shear sideways by whole blocks, some bands only,
//                        which band shifting re-rolled each phase. The instant "this signal
//                        is broken" read.
//   2. HOT CORRUPTION  — scattered nibble-blocks flip from the base ink to `hot` (bad
//                        memory lit up), and a few drop out to nothing (a hole in the frame).
//   3. EDGE GHOST      — a torn band leaves a one-block `hot` fringe at its leading edge, the
//                        chromatic-split tell of a mis-clocked scanline.
//
// Stateless, like the dissolve family it borrows its hash from (dissolve.h): a pure function
// of the source pixel and `phase`, allocating nothing and drawing the same frame twice for
// the same phase — so a ~4fps event-driven repaint can run it and the frame gates can dump
// it. `phase` is the animation clock (typically an anim-beat scaled into 0..255); step it and
// the corruption re-rolls, hold it and the frame is frozen.
//
// Shape only comes from the sprite's alpha, exactly as drawSpriteTinted/drawShred read it, so
// FX_GLITCH works on any sprite form but is MEANT for the one-ink malbeast masks: `base` is
// the colour a set bit takes (the body), `hot` the corruption accent. On a full-colour sprite
// it would flatten the body to `base`, which is not what it is for.
#pragma once

#include <cstdint>

#include "core/render/color.h"

namespace mal {

class Framebuffer;
struct SpriteData;

// `s`/`frame`/`row` is the subject, seated top-left at (destX, destY) and scaled num/den the
// way drawSpriteUpscaled would seat it. `base` fills the silhouette, `hot` is the corruption
// accent. `phase` (0..255) is the glitch clock. `mirror` turns the drawing in its cell
// (spriteSrcX) for a malbeast seated facing the other way.
void drawSpriteGlitch(Framebuffer& fb, const SpriteData& s, int frame,
                      int destX, int destY, int num, int den,
                      Rgb565 base, Rgb565 hot, uint8_t phase,
                      int row = 0, bool mirror = false);

} // namespace mal
