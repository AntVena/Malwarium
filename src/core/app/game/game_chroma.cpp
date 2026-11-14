#include "core/app/game.h"

#include <cstdio>

#include "tunables.h"
#include "core/content/creatures/creature_lines.h"

// game_chroma.cpp: the CHROMATOPHORE, the Metamorphic line's hatch minigame.
//
// A Polystaria egg is a translucent bell with nothing of its own to hide behind, so the
// moment it is laid it rehearses the only defence its line has. The water under it takes
// one of three colours, A/B/C wear one skin each, and a sweep crosses the panel on a
// shrinking clock: be wearing the water when it arrives and the pass is HIDDEN, be
// caught in the wrong skin (or halfway through the change), and the run is over with
// whatever it already earned.
//
// It is the line's own argument played as a game. Every other hatch minigame is about
// finding something (the live egg, the key, the byte); this one is about BECOMING
// something, in time, which is the only thing this family does that the others don't.
//
// THE SAME COLOUR TWICE. A skin is one PAL_CORE token used for both halves of the
// match: camoRampFromTone builds the water's tones AND the palette FX_CAMO repaints the
// creature in, so "wearing it" is literally the same ramp on both, and a retune of the
// hue can never desynchronise the two. It costs one call: the pet is drawn with
// drawSpriteCamo instead of drawSpriteUpscaled, and the level it is drawn at is the
// model's own settle (Chromatophore::wearPct) rather than a second animation clock, so
// the scatter crawling across the creature IS the thing the sweep is about to score.
//
// GRAYSCALE. The three skins are a luminance ladder (assets/PAL_CORE.json's camo block)
// and the water is TEXTURED per skin besides (streaks, stipple, bands), so which skin
// is down there survives the colour being taken away, and the chips below name all
// three in words. Colour is the fast channel here, never the only one.
//
// The rules are core/model/chromatophore.h, with no framebuffer and no Game in them,
// and the board's input and draw are ChromaScreen (game/chroma_screen.h). This file is
// who stands on the board, the start and the payout.
//
// Entered from Game::startHatchGame the instant the egg is laid; leaves to idle, or
// straight into the hatch when the passes finish the clock.

namespace mal {


// --- Who is standing on the board ------------------------------------------

const CreatureDef* Game::chromaSubject() const {
    // The pet, whenever the pet is something that plausibly does this. Every hatch route
    // in lands here (the egg on the shelf IS the pet, and it is on the family that
    // wears borrowed colours), so the fallback below only ever answers for an arcade
    // cabinet played by somebody else's pet.
    if (pet_.creature) {
        const CreatureLine* fam = creatureLine(pet_.creature->line);
        if (fam && fam->wearsBorrowedColours) return pet_.creature;
    }
    // Otherwise the egg belonging to whichever line runs this game, asked for by its
    // HatchGame rather than by name: the cabinet is a rehearsal, and what rehearses is
    // whatever creature the board was built for.
    for (const EggLineDef* line : registry_.allEggLines()) {
        if (line && line->hatchGame == HatchGame::Chroma)
            return registry_.creature(line->eggCreatureId);
    }
    return nullptr;
}

// --- Lifecycle -------------------------------------------------------------

void Game::startChroma(int rounds, int windowMs, bool switching) {
    rng_ = rng_ * 1664525u + 1013904223u;   // the shared LCG seeds the skin sequence
    chroma_.reset(rng_, rounds, static_cast<uint32_t>(windowMs),
                  static_cast<uint32_t>(kChromaWindowStepMs), switching);
    chromaBanked_ = 0;
    lastChromaMs_ = nowMs_;
    closeGameBrief();   // a stale flag from a previous run must not open paused
    nav_ = Nav::Chroma;
    dirty_ = true;
}

void Game::finishChroma() {
    // An arcade run has no clock to spend the passes on: the till takes the score and
    // that is the whole settlement. The cabinet run is endless, so it has no clean()
    // result. A win is reaching kArcadeChromaWinPasses, and the score can go past it.
    if (inArcadeRun()) {
        finishArcadeRun(chroma_.passes() >= kArcadeChromaWinPasses, chroma_.passes(),
                        kArcadeChromaWinPasses);
        return;
    }
    // Spend the run. Banked against chromaBanked_ rather than paid straight out, so a
    // second press on the result screen can't buy the same passes twice.
    const int owed = chroma_.passes() - chromaBanked_;
    if (owed > 0) {
        chromaBanked_ = chroma_.passes();
        accelerateEggHatch(static_cast<uint32_t>(owed) * kChromaPassMs);
    }
    // NEVER_SEEN asks for the run that was never spotted, and this is the one place it
    // exists: a clean board, not one that merely earned a lot. The Worm's own clean-run
    // row (WORM_WHISPERER) is the same idea on the same seam.
    if (chroma_.clean()) unlockAchievement(ach::kNeverSeen);
    if (pet_.bootHatchRemainMs == 0) {
        completeHatch();   // nothing left to incubate: hatch here, not a tick later
        return;
    }
    nav_ = Nav::Idle;
    dirty_ = true;
    markSaveDirty();
}

}  // namespace mal
