// glitch.h — FX_GLITCH: the wild-malbeast corruption shader.
//
// A malbeast's sprite is a cheap two-tone blocky drawing (tools/gen_malbeast_art.py): light
// line work on a dark body, two poses, no motion of its own. The corruption is this pass,
// applied at draw time rather than stored in the pixels, so every malbeast shares one
// animated treatment and each sheet stays a cheap still.
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
// Shape comes from the sprite's alpha, exactly as drawSpriteTinted/drawShred read it, and
// colour from the sprite itself — the malbeasts' two tones, line and body — with the same
// split drawShred makes: a one-ink MASK has no colour of its own and takes `base`. `hot` is
// the corruption accent either way.
//
// The combat screen runs it only inside the bursts glitchBurst() schedules and draws the
// wild clean between them; torn on every beat, the drawing underneath does not read.
#pragma once

#include <cstdint>

#include "core/render/color.h"

namespace mal {

class Framebuffer;
struct SpriteData;

// `s`/`frame`/`row` is the subject, seated top-left at (destX, destY) and scaled num/den the
// way drawSpriteUpscaled would seat it. `base` colours a one-ink mask (a sheet with colours
// of its own keeps them), `hot` is the corruption accent. `phase` (0..255) is the glitch
// clock. `mirror` turns the drawing in its cell (spriteSrcX) for a malbeast seated facing
// the other way.
void drawSpriteGlitch(Framebuffer& fb, const SpriteData& s, int frame,
                      int destX, int destY, int num, int den,
                      Rgb565 base, Rgb565 hot, uint8_t phase,
                      int row = 0, bool mirror = false);

// The burst schedule: one window every kGlitchBurstPeriod beats, torn for kGlitchBurstLen
// of them. Where in its window a burst falls is rolled per window off the shared scatter,
// so the faults land irregularly rather than on a metronome — and, like the shader, it is a
// pure function of the beat, so a repaint or a frame gate sees the same schedule.
//
// In beats of whatever clock the caller passes; on the combat stage (~8fps, kCombatAnimMs)
// that is a quarter-second tear in each second and a half.
constexpr int kGlitchBurstPeriod = 12;
constexpr int kGlitchBurstLen = 2;
bool glitchBurst(int beat);

} // namespace mal
