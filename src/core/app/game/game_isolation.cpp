#include "core/app/game.h"

#include <cstdio>

#include "tunables.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

// game_isolation.cpp — the Isolation Protocol, the Worm line's hatch minigame.
//
// A Vermicell egg is laid and the worm inside it is turned straight loose in a
// quarantine buffer: it never stops moving, A steers it left, B steers it right, and
// every byte it swallows is kIsolationDotMs off the incubation clock. Crash into a wall
// or into its own body and the run banks what it earned; swallow the whole clock and the
// protocol finished CLEAN — the egg hatches out of the minigame and WORM_WHISPERER
// fires. A bad run costs only the bonus, never the pet, which is the same deal the
// Clutch Pick and the defrag Stacker both offer.
//
// The rules are core/model/isolation.h, with no framebuffer and no Game in them. This
// file is the screen, the cadence and the payout.
//
// Entered from Game::startHatchGame the instant the egg is laid; leaves to idle, or
// straight into the hatch when the clock is gone.

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

// --- Lifecycle -------------------------------------------------------------

void Game::startIsolation() {
    // The goal is the WHOLE incubation clock, priced in bytes — so "clean" means the run
    // that hatched the egg by itself, and the achievement has something exact to test.
    // Rounded up, so a clock that isn't a whole number of minutes still has to be
    // finished rather than merely reached.
    const int goal =
        static_cast<int>((bootHatchRemainMs_ + kIsolationDotMs - 1) / kIsolationDotMs);
    rng_ = rng_ * 1664525u + 1013904223u;   // the shared LCG seeds the byte sequence
    isolation_.reset(rng_, goal);
    isolationBanked_ = 0;
    lastIsolationStepMs_ = nowMs_;
    nav_ = Nav::Isolation;
    dirty_ = true;
}

void Game::onIsolation(const ButtonEvent& ev) {
    if (!isolation_.running()) {
        // Parked on the result. B banks and leaves; C is DISABLED, like every other
        // hatch screen — there is no pet to go back to, only an egg to get on with.
        if (ev.button == Button::B) finishIsolation();
        return;
    }
    // A and B STEER, which is the deviation from the standard A/B/C contract this screen
    // spells out in its hint band. C is inert: a run ends by crashing or by finishing,
    // and quitting out of one would be a third way with nothing to say about it.
    if (ev.button == Button::A) isolation_.turnLeft();
    else if (ev.button == Button::B) isolation_.turnRight();
    dirty_ = true;
}

void Game::finishIsolation() {
    // Spend the run. Banked against isolationBanked_ rather than paid straight out, so a
    // second press on the result screen can't buy the same bytes twice.
    const int owed = isolation_.dots() - isolationBanked_;
    if (owed > 0) {
        isolationBanked_ = isolation_.dots();
        accelerateEggHatch(static_cast<uint32_t>(owed) * kIsolationDotMs);
    }
    // WORM_WHISPERER asks for a CLEAN protocol, and this is the one place that exists:
    // the run that ate the entire clock, not the one that ran out of buffer first.
    if (isolation_.clean()) unlockAchievement(ach::kWormWhisperer);
    if (bootHatchRemainMs_ == 0) {
        completeHatch();   // nothing left to incubate — hatch here, not a tick later
        return;
    }
    nav_ = Nav::Idle;
    dirty_ = true;
    markSaveDirty();
}

// --- Render ----------------------------------------------------------------

void Game::drawIsolation(Framebuffer& fb) const {
    fb.clear(palColor(Pal::PAPER));

    const char* title = "ISOLATION PROTOCOL";
    drawText(fb, (kActiveW - textWidth(title)) / 2, kTitleY, title, palColor(Pal::INK));

    // The buffer wall, drawn as a frame OUTSIDE the play area so a cell touching it is
    // still a whole cell — the wall is lethal, so where it starts has to be exact.
    const Rgb565 ink = palColor(Pal::INK);
    fb.fillRect(kBoardX - kWall, kBoardY - kWall, kBoardW + 2 * kWall, kWall, ink);
    fb.fillRect(kBoardX - kWall, kBoardY + kBoardH, kBoardW + 2 * kWall, kWall, ink);
    fb.fillRect(kBoardX - kWall, kBoardY, kWall, kBoardH, ink);
    fb.fillRect(kBoardX + kBoardW, kBoardY, kWall, kBoardH, ink);

    // The loose byte: a HOLLOW square, against the worm's solid ones. Solid-vs-hollow is
    // the one difference that holds at any brightness, which matters more here than
    // anywhere else on the screen — the byte is the only thing the player is aiming at.
    if (isolation_.running()) {
        const int bx = cellX(isolation_.dot()) + 2, by = cellY(isolation_.dot()) + 2;
        fb.fillRect(bx, by, 8, 2, ink);
        fb.fillRect(bx, by + 6, 8, 2, ink);
        fb.fillRect(bx, by, 2, 8, ink);
        fb.fillRect(bx + 6, by, 2, 8, ink);
    }

    // The worm, tail first so the head lands on top of its own neck. Body cells are
    // inset by a pixel, which leaves the seam between two segments visible and makes the
    // coil countable; the HEAD fills its cell edge to edge. That size difference is what
    // carries the head in grayscale — the ACCENT on it only repeats what the shape
    // already says, and accent is this screen's focus, not a status.
    for (int i = isolation_.length() - 1; i > 0; --i)
        fb.fillRect(cellX(isolation_.segment(i)) + 1, cellY(isolation_.segment(i)) + 1,
                    kCell - 2, kCell - 2, ink);
    fb.fillRect(cellX(isolation_.head()), cellY(isolation_.head()), kCell, kCell,
                palColor(Pal::ACCENT));

    if (!isolation_.running()) {
        const bool clean = isolation_.clean();
        const char* verdict = clean ? "BUFFER CLEAN" : "COLLISION - RUN ENDED";
        drawText(fb, (kActiveW - textWidth(verdict)) / 2, kVerdictY, verdict, ink);

        char effect[28];
        if (clean) std::snprintf(effect, sizeof(effect), "HATCHING NOW");
        else std::snprintf(effect, sizeof(effect), "-%d MIN INCUBATION",
                           isolation_.dots());
        drawText(fb, (kActiveW - textWidth(effect)) / 2, kEffectY, effect,
                 isolation_.dots() > 0 ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM));

        const char* hint = "B CONTINUE   C DISABLED";
        drawText(fb, (kActiveW - textWidth(hint)) / 2, kHintY, hint, palColor(Pal::INK_DIM));
        return;
    }

    // What the run is worth so far, and what is left to make it clean. Both in the same
    // unit the prize is paid in, so the player is reading the incubation clock rather
    // than a score they'd have to convert.
    char status[32];
    std::snprintf(status, sizeof(status), "-%d MIN   %d TO CLEAN", isolation_.dots(),
                  isolation_.goal() - isolation_.dots());
    drawText(fb, (kActiveW - textWidth(status)) / 2, kEffectY, status, ink);

    // UI_HINT_BAND. A and B steer here instead of stepping and accepting, so the band
    // names the turn each one makes and sits them the way the buttons do, A left of B.
    const char* hint = "A LEFT   B RIGHT";
    drawText(fb, (kActiveW - textWidth(hint)) / 2, kHintY, hint, palColor(Pal::INK_DIM));
}

}  // namespace mal
