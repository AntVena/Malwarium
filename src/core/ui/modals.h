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

// Evolution boundary: the celebratory Stage-advance cinematic. B continues (host side);
// C is disabled (nothing to cancel). Grayscale-safe.
//
// Hold shows the pre-evolution `from` sprite, Flash the FX_EVO_FLASH white-out, and
// Reveal the `to` sprite + `toName` + the UI_STAGE_INDICATOR lit to `toStage`.
enum class EvolvePhase : uint8_t { Hold, Flash, Reveal };
struct EvolveView {
    EvolvePhase phase = EvolvePhase::Hold;
    const SpriteData* from = nullptr;
    const SpriteData* to = nullptr;
    const char* toName = "";
    Stage toStage = Stage::Process;
    // What the reveal says CHANGED, under the stage bar: the care path the pet grew up
    // on, and a move slot it opened (null = draw nothing). A new form alone does not say
    // what it means for the pet.
    const char* pathLine = nullptr;
    const char* slotLine = nullptr;
    int beat = 0;
};
void drawEvolveModal(Framebuffer& fb, const EvolveView& v);

// Critical System Failure: the terminal death modal. The dying `pet`
// corrupts out under a FX_CRITICAL_FAIL crash overlay (glitch bands = the
// grayscale-safe channel). The B-acknowledge hint waits for Held, once the crash has
// held; C is disabled (death is not cancellable). Grayscale-safe.
enum class CsfPhase : uint8_t { Crashing, Held };
void drawCSFModal(Framebuffer& fb, const SpriteData* pet, CsfPhase phase, int beat);

// Feeding beat: the pet eats, and a gauge reads back each stat the food
// touched. The rows are derived from `food`'s own ItemEffect list
// (core/content/defs.h), so a morsel that only lifts Happiness never shows a
// Hunger bar it never touched, and the arrow comes from `before` vs. the live
// model — a stat the food had no room left to move says so by having no arrow.
// `food` may be null (nothing to report).
//
struct FeedingView {
    const SpriteData* pet = nullptr;
    const ItemDef* food = nullptr;
    // The item's own ICON_ITEM_* glyph (items_screen.h's itemIcon), which the pet visibly
    // eats: it comes apart into blocks and streams in over kAbsorbBeats (FX_ABSORB,
    // core/render/absorb.h) while the gauges settle underneath. Null draws the beat
    // without it: the modal does not depend on the picture.
    const SpriteData* foodIcon = nullptr;
    FeedVitals before;
    // The heartbeat (the pet's breathe), and the faster dissolve clock (Game::fxBeat_)
    // the bite is paced on. They are different rates, so a smoother sweep never speeds
    // up the creature breathing behind it.
    int beat = 0;
    int fxBeat = 0;
};
void drawFeedingModal(Framebuffer& fb, const PetModel& m, const FeedingView& v);

// Lockout Timer: flashing countdown, a two-path resolve choice (Feed <-> Pay Bits),
// C disabled.
enum class LockoutPath : uint8_t { Feed, Pay };
struct LockoutView {
    const SpriteData* pet = nullptr;
    int secondsLeft = 0;             // the numeric readout
    float remainFrac = 0.0f;         // 1..0, drains the countdown bar
    LockoutPath focus = LockoutPath::Feed;
    int bitsCost = 0;
    bool canPay = true;              // false greys the Pay path when Bits are short
    // False when the bag holds nothing that resolves a Lockout: the feed row becomes the
    // place food comes from, dimmed, so the crisis says why that path is shut.
    bool hasFood = true;
    // >= 0 means the pet is already FAILING (5/5 errors) with that much of its window
    // left: feeding still clears the Lockout but no longer saves the pet, so the headline
    // becomes the countdown and the feed row says what feeding will and will not do.
    int32_t failingLeftMs = -1;
    // A Yubi-Cookie this pet can still eat is in the bag: the one rescue a failing pet has.
    bool yubiReady = false;
    int beat = 0;
};
void drawLockoutModal(Framebuffer& fb, const LockoutView& v);

} // namespace mal
