// modals.h — event overlays: full-screen modals
// that preempt the carousel tree: Feeding, Lockout, Hatch, Line-select,
// Evolution, and Critical System Failure.
#pragma once

#include "core/content/defs.h"
#include "core/model/pet_model.h"
#include "core/ui/ui_state.h"  // FeedVitals

namespace mal {

class Framebuffer;
struct SpriteData;

// Evolution boundary: the celebratory Stage-advance cinematic. `phase`
// runs 0 hold (the pre-evolution `from` sprite) -> 1 FX_EVO_FLASH white-out -> 2 reveal
// (`to` sprite + `toName` + the UI_STAGE_INDICATOR lit to `toStage`). B continues
// (host side); C is disabled (nothing to cancel). Grayscale-safe.
void drawEvolveModal(Framebuffer& fb, const SpriteData* from, const SpriteData* to,
                     const char* toName, Stage toStage, int phase, int beat);

// Critical System Failure: the terminal death modal. The dying `pet`
// corrupts out under a FX_CRITICAL_FAIL crash overlay (glitch bands = the
// grayscale-safe channel). `revealed` gates the B-acknowledge hint until the crash
// has held; C is disabled (death is not cancellable). Grayscale-safe.
void drawCSFModal(Framebuffer& fb, const SpriteData* pet, bool revealed, int beat);

// Feeding beat: the pet eats, and a gauge reads back each stat the food
// touched. The rows are derived from `food`'s own ItemEffect list
// (core/content/defs.h), so a morsel that only lifts Happiness never shows a
// Hunger bar it never touched, and the arrow comes from `before` vs. the live
// model — a stat the food had no room left to move says so by having no arrow.
// `food` may be null (nothing to report).
//
// `foodIcon` is the item's own ICON_ITEM_* glyph (items_screen.h's itemIcon), which
// the pet visibly eats: it comes apart into blocks and streams in over kAbsorbBeats
// (FX_ABSORB, core/render/absorb.h) while the gauges settle underneath. Null draws the
// beat without it — the modal has never depended on the picture.
// `beat` is the heartbeat (the pet's breathe); `fxBeat` is the faster dissolve clock
// (Game::fxBeat_) the bite is paced on — they are different rates, so a smoother sweep
// never speeds up the creature breathing behind it.
void drawFeedingModal(Framebuffer& fb, const SpriteData* pet, const SpriteData* foodIcon,
                      const ItemDef* food, const PetModel& m, const FeedVitals& before,
                      int beat, int fxBeat);

// Lockout Timer: flashing countdown, a two-path resolve choice
// (Open Items <-> Pay Bits), C disabled. `payOption` selects which path is
// focused; `canPay` greys the Pay path when Bits are short. `remainFrac` (1..0)
// drains the countdown bar; `secondsLeft` is the numeric readout.
void drawLockoutModal(Framebuffer& fb, const SpriteData* pet, const PetModel& m,
                      int secondsLeft, float remainFrac, bool payOption,
                      bool canPay, int bitsCost, int beat);

} // namespace mal
