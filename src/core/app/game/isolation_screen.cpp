#include "core/app/game/isolation_screen.h"

#include <cstdio>

#include "tunables.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/model/isolation.h"

namespace mal {

namespace {

// The buffer's geometry, in ACTIVE pixels. Engine fills at x1 rather than the usual
// x1.75 upscale, the way the Clutch panel is drawn: nothing here is art, and a 12px cell
// keeps the worm, the wall and the byte all on whole pixels. 16x11 cells is 192x132,
// which centres with room above for the title and below for the two text rows.
constexpr int kCell = 12;
constexpr int kBoardW = kIsolationCols * kCell;
constexpr int kBoardH = kIsolationRows * kCell;
constexpr int kBoardX = (kActiveW - kBoardW) / 2;
constexpr int kBoardY = 46;
constexpr int kWall = 2;          // the buffer wall, drawn just outside the play area

// Rows shared with the Clutch Pick, so the two hatch minigames sit their text in the
// same places (title, verdict, effect, UI_HINT_BAND).
constexpr int kTitleY = 14;
constexpr int kVerdictY = 30;
constexpr int kEffectY = 186;
constexpr int kHintY = 204;

int cellX(int cell) { return kBoardX + (cell % kIsolationCols) * kCell; }
int cellY(int cell) { return kBoardY + (cell / kIsolationCols) * kCell; }

}  // namespace

void IsolationScreen::onButton(IsolationHost& host, const ButtonEvent& ev) {
    if (host.onGameBriefInput(ev)) return;
    Isolation& run = host.liveIsolation();
    if (!run.running()) {
        // Parked on the result. B banks and leaves. C is disabled, as on every
        // hatch screen, because there is no pet to go back to.
        if (ev.button == Button::B) host.finishIsolation();
        return;
    }
    // A and C steer (the outer buttons, so left maps to left), and the hint band says so
    // because it departs from the standard A/B/C contract. B is inert while the run is
    // live: a run ends by crashing or by finishing, and quitting out of one would be a
    // third way with nothing to say about it. B comes back as CONTINUE on the verdict
    // above.
    if (ev.button == Button::A) run.turnLeft();
    else if (ev.button == Button::C) run.turnRight();
}

// --- Render ----------------------------------------------------------------

void IsolationScreen::draw(Framebuffer& fb, const IsolationHost& host) const {
    if (host.gameBriefOpen()) { host.drawGameBrief(fb); return; }
    const Isolation& run = host.isolation();
    fb.clear(palColor(Pal::PAPER));

    const char* title = "ISOLATION PROTOCOL";
    drawText(fb, (kActiveW - textWidth(title)) / 2, kTitleY, title, palColor(Pal::INK));

    // The buffer wall, drawn as a frame OUTSIDE the play area so a cell touching it is
    // still a whole cell: the wall is lethal, so its edge has to be exact.
    const Rgb565 ink = palColor(Pal::INK);
    fb.fillRect(kBoardX - kWall, kBoardY - kWall, kBoardW + 2 * kWall, kWall, ink);
    fb.fillRect(kBoardX - kWall, kBoardY + kBoardH, kBoardW + 2 * kWall, kWall, ink);
    fb.fillRect(kBoardX - kWall, kBoardY, kWall, kBoardH, ink);
    fb.fillRect(kBoardX + kBoardW, kBoardY, kWall, kBoardH, ink);

    // The loose byte: a HOLLOW square, against the worm's solid ones. Solid against
    // hollow holds at any brightness, which matters most for the byte, the player's
    // target.
    if (run.running()) {
        const int bx = cellX(run.dot()) + 2, by = cellY(run.dot()) + 2;
        fb.fillRect(bx, by, 8, 2, ink);
        fb.fillRect(bx, by + 6, 8, 2, ink);
        fb.fillRect(bx, by, 2, 8, ink);
        fb.fillRect(bx + 6, by, 2, 8, ink);
    }

    // The worm, tail first so the head lands on top of its own neck. Body cells are
    // inset by a pixel, which leaves the seam between two segments visible and makes the
    // coil countable; the HEAD fills its cell edge to edge. The size difference
    // identifies the head in grayscale; the ACCENT only repeats it, as this screen's
    // focus colour.
    for (int i = run.length() - 1; i > 0; --i)
        fb.fillRect(cellX(run.segment(i)) + 1, cellY(run.segment(i)) + 1,
                    kCell - 2, kCell - 2, ink);
    fb.fillRect(cellX(run.head()), cellY(run.head()), kCell, kCell,
                palColor(Pal::ACCENT));

    if (!run.running()) {
        const bool clean = run.clean();
        const char* verdict = clean ? "BUFFER CLEAN" : "COLLISION - RUN ENDED";
        drawText(fb, (kActiveW - textWidth(verdict)) / 2, kVerdictY, verdict, ink);

        // The effect line is priced in incubation, which an arcade run lacks, so
        // there it reports the buffer and the payout screen shows the money.
        char effect[28];
        if (host.inArcadeRun())
            std::snprintf(effect, sizeof(effect), "%d / %d BYTES", run.dots(),
                          run.goal());
        else if (clean) std::snprintf(effect, sizeof(effect), "HATCHING NOW");
        else if (run.dots() == 0)
            std::snprintf(effect, sizeof(effect), "HATCHES ON ITS OWN CLOCK");
        else std::snprintf(effect, sizeof(effect), "HATCHES %d MIN SOONER",
                           run.dots());
        drawText(fb, (kActiveW - textWidth(effect)) / 2, kEffectY, effect,
                 run.dots() > 0 ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM));

        const char* hint = "B CONTINUE   C DISABLED";
        drawText(fb, (kActiveW - textWidth(hint)) / 2, kHintY, hint, palColor(Pal::INK_DIM));
        return;
    }

    // What the run is worth so far, and what is left to make it clean. Both in the same
    // unit the prize is paid in, so the player is reading the incubation clock rather
    // than a score they'd have to convert.
    char status[32];
    if (host.inArcadeRun())
        std::snprintf(status, sizeof(status), "%d BYTES   %d TO CLEAN",
                      run.dots(), run.goal() - run.dots());
    else
        std::snprintf(status, sizeof(status), "-%d MIN   %d TO CLEAN", run.dots(),
                      run.goal() - run.dots());
    drawText(fb, (kActiveW - textWidth(status)) / 2, kEffectY, status, ink);

    // UI_HINT_BAND. A and C steer here instead of stepping and cancelling, so the band
    // names the turn each one makes and sits them the way the buttons do: A on the
    // left of the device turns left, C on the right turns right.
    const char* hint = "A LEFT   C RIGHT";
    drawText(fb, (kActiveW - textWidth(hint)) / 2, kHintY, hint, palColor(Pal::INK_DIM));
}

}  // namespace mal
