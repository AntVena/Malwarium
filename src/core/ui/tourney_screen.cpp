#include "core/ui/tourney_screen.h"

#include "core/model/tournament.h"   // tourneyBlockSize / tourneyBlockStart — the tree the ties draw
#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/sprite.h"
#include "core/render/palette.h"

namespace mal {

void drawDockTies(Framebuffer& fb, uint8_t alive, int cursor, int round,
                  int playerSlot) {
    for (int r = 0; r < kTourneyRounds; ++r) {
        const int n = tourneyBlockSize(r);
        const int x = dockTieX(r);
        const Rgb565 c = r == round ? palColor(Pal::INK) : palColor(Pal::INK_DIM);
        for (int start = 0; start < kTourneySlots; start += n) {
            const int yA = dockBlockAnchorY(alive, start, n / 2, cursor);
            const int yB = dockBlockAnchorY(alive, start + n / 2, n / 2, cursor);
            const int t =
                playerSlot >= 0 && tourneyBlockStart(playerSlot, r) == start ? 2 : 1;
            fb.fillRect(x, yA, t, yB - yA + t, c);       // the spine
            fb.fillRect(x, yA, kDockTieColW, t, c);      // an arm into each half
            fb.fillRect(x, yB, kDockTieColW, t, c);
        }
    }
}

int dockFieldBottomMax(uint8_t alive) {
    // The tallest the field can get is with the cursor expanding one out row. Found by
    // asking, rather than by reasoning about which slot it is: the answer is one loop
    // over eight, and a rule derived twice is a rule that eventually disagrees.
    int worst = dockFieldBottom(alive, -1);
    for (int slot = 0; slot < kTourneySlots; ++slot) {
        const int b = dockFieldBottom(alive, slot);
        if (b > worst) worst = b;
    }
    return worst;
}

bool dockFaceoffFits(uint8_t alive) {
    return dockFieldBottomMax(alive) + 4 <= kDockFighterY;
}

int dockSeatX(const SpriteData& s, bool mirror) {
    return kActiveW - kMargin - spriteContentX1(s, mirror);
}

int dockSeatY(const SpriteData& s) { return kDockFeetY - s.h; }

int dockCardW(const SpriteData* s, bool mirror) {
    const int left = s ? dockSeatX(*s, mirror) + spriteContentX0(*s, mirror) - 4
                       : kActiveW - kMargin;
    return left - kMargin;
}

}  // namespace mal
