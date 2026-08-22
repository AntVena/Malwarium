// chromatophore.h — the rules of the CHROMATOPHORE, with no rendering and no Game.
//
// The Metamorphic egg's hatch minigame. The bell is a translucent bag with nothing of
// its own to hide behind, so before it hatches it rehearses the only defence the line
// has: wearing somebody else's colours. The water it is drifting over takes one of
// kChromaSkins colours, one button per skin repaints the pet, and a sweep crosses the
// panel on a clock. Be wearing the water when the sweep arrives and the pass is
// HIDDEN; be wearing anything else — or be caught halfway through the change — and the
// run is SPOTTED and over, keeping whatever it already earned. That last-clause deal is
// the same one the Isolation buffer and the Defrag board offer: a bad run costs the
// bonus, never the pet.
//
// A CHANGE TAKES TIME, and that is the whole game. Pressing the right button is not the
// skill — kChromaSettleMs later is when the pet is actually wearing it, so the question
// each round asks is whether you committed early enough, and a late press is caught
// visibly mid-repaint rather than by an invisible rule. The settle is therefore a rule
// of the board and lives here, not a render magnitude: the screen draws the same number
// as FX_CAMO's level (core/render/camo.h), so what the player sees scattering across
// the creature IS what the sweep is about to score.
//
// The plate never repeats what the pet is already wearing, so no round is free.
//
// SEEDED, not deterministic — the Isolation precedent. A fixed sequence of skins would
// be memorised into a rhythm by the second egg, so the caller passes a seed (Game hands
// it one off the shared LCG) and a test can still pin a run exactly.
//
// Kept a plain model (core/model, like stacker/isolation/disk_decryption) so the rules
// can be tested a press at a time without a framebuffer or a Game. It deals in skin
// INDEXES and milliseconds only; which colour a skin is, and what a pass is worth, are
// both the caller's business (game_chroma.cpp, and tunables.h's kChromaPassMs).
#pragma once

#include <cstdint>

namespace mal {

// Three skins, three buttons — the board's size is the device's. A fourth would have
// nowhere to live: A/B/C is the whole input surface, and a game whose choices outnumber
// the buttons has to add a cursor, which is exactly the deliberation this game exists
// not to allow time for.
constexpr int kChromaSkins = 3;

// How long a repaint takes to settle, from the press to fully worn. Four ~4fps frames,
// which is FX_CAMO's own kCamoFadeStep sweep (255 in four steps) expressed as time: the
// scatter has to be legible AS a scatter, or "caught halfway" reads as the board
// cheating rather than as the player being late.
constexpr uint32_t kChromaSettleMs = 1000;

// The tightest a window may get, however many rounds a run asks for. Below about this
// there is no time to read the plate and commit, and the run stops being a game about
// choosing early and becomes one about reflexes.
constexpr uint32_t kChromaWindowFloorMs = 1800;

// Where in the window a SWITCHING round changes the water under the pet — late enough
// that the first disguise had to be real, early enough that the second one can still be
// finished inside kChromaSettleMs.
constexpr int kChromaSwitchAtPct = 40;

class Chromatophore {
  public:
    enum class State : uint8_t {
        Running,
        Spotted,   // the sweep found it — banks the passes already made
        Clean,     // every round survived: the whole run
    };

    Chromatophore() { reset(1, 1, 3000, 0, /*switching=*/false); }

    // Start a fresh run. `windowMs` is how long the first round gives between the water
    // changing and the sweep arriving, and `windowStepMs` is how much of that each
    // later round takes away — the pace dial, and the only difference between the hatch
    // and an arcade cabinet's EASY. `switching` lets a round change the water once
    // mid-window, which is the HARD setting and off everywhere else.
    //
    // `rounds` of 0 or less is ENDLESS: there is no round the run finishes on, so it
    // ends the only other way it can, by being spotted. That is the arcade's shape and
    // not the hatch's — an egg is buying a fixed number of minutes and wants a finish
    // line, while a cabinet is a high score and a finish line would be a ceiling on it.
    // An endless run has no clean(), which is the honest answer: nothing was completed.
    void reset(uint32_t seed, int rounds, uint32_t windowMs, uint32_t windowStepMs,
               bool switching) {
        rng_ = seed ? seed : 1u;
        goal_ = rounds < 1 ? 0 : rounds;
        window0_ = windowMs;
        windowStep_ = windowStepMs;
        switching_ = switching;
        passes_ = 0;
        round_ = 0;
        state_ = State::Running;
        // The pet opens already wearing skin 0, settled. A run has to start from a
        // KNOWN skin — that is what lets the first plate be something else, so the
        // opening round is a real change like every other one.
        worn_ = 0;
        leaving_ = 0;
        wearMs_ = kChromaSettleMs;   // settled in what it already is
        beginRound();
    }

    // A button press: wear skin `s`. Pressing what the pet already wears is inert
    // rather than a re-start — a nervous double-press must not undo a settled disguise.
    void wear(int s) {
        if (state_ != State::Running) return;
        if (s < 0 || s >= kChromaSkins || s == worn_) return;
        leaving_ = worn_;
        worn_ = static_cast<uint8_t>(s);
        wearMs_ = 0;
    }

    // Advance the run by `dtMs` of real time: the repaint settles, the sweep travels,
    // and a round that runs out of window resolves. Inert once the run is over.
    void tick(uint32_t dtMs) {
        if (state_ != State::Running) return;
        wearMs_ += dtMs;
        if (wearMs_ > kChromaSettleMs) wearMs_ = kChromaSettleMs;
        elapsed_ += dtMs;
        if (switching_ && !switched_ &&
            elapsed_ >= window_ * static_cast<uint32_t>(kChromaSwitchAtPct) / 100u) {
            switched_ = true;
            plate_ = pickSkin(plate_);
        }
        if (elapsed_ < window_) return;
        // The sweep is here. It scores what is on the creature RIGHT NOW, which is why
        // a half-finished change fails: the state it reads is the state it can see.
        if (!hidden()) { state_ = State::Spotted; return; }
        ++passes_;
        ++round_;
        if (goal_ > 0 && round_ >= goal_) { state_ = State::Clean; return; }
        beginRound();
    }

    int plate() const { return plate_; }
    int worn() const { return worn_; }
    // The skin being left behind. Meaningless once wearPct() reaches 100 — nothing is
    // being left any more — and there only so the screen can draw the repaint as a
    // dissolve between two skins rather than through the creature's own colours.
    int leaving() const { return leaving_; }
    // How far the repaint has settled, 0..100 — the screen's FX_CAMO level, and the
    // half of `hidden` the player can actually watch.
    int wearPct() const {
        return static_cast<int>(wearMs_ * 100u / kChromaSettleMs);
    }
    // Where the sweep is across the panel, 0..100.
    int sweepPct() const {
        return window_ ? static_cast<int>(elapsed_ * 100u / window_) : 100;
    }
    // Wearing the water, and finished doing it. What the sweep asks.
    bool hidden() const { return worn_ == plate_ && wearMs_ >= kChromaSettleMs; }

    int passes() const { return passes_; }
    // Passes that would finish the run, or 0 for an endless one — so a caller printing a
    // "3 / 10" has to ask whether there is a denominator at all.
    int goal() const { return goal_; }
    bool endless() const { return goal_ <= 0; }
    int round() const { return round_; }     // rounds already survived, 0..goal
    State state() const { return state_; }
    bool running() const { return state_ == State::Running; }
    bool clean() const { return state_ == State::Clean; }

  private:
    void beginRound() {
        elapsed_ = 0;
        switched_ = false;
        // Every round is a real change: the water never takes the colour the pet is
        // already standing in, so there is no round that plays itself.
        plate_ = pickSkin(worn_);
        // The ramp keeps shedding on an endless run too — it just runs out of window to
        // shed and settles at the floor, which is where a high score is actually played.
        const uint32_t shed = windowStep_ * static_cast<uint32_t>(round_);
        window_ = window0_ > shed + kChromaWindowFloorMs ? window0_ - shed
                                                         : kChromaWindowFloorMs;
    }

    // A skin that is not `avoid`. Drawn rather than cycled, so a run has no rhythm to
    // learn, and never the one to avoid, so a round always demands a repaint.
    uint8_t pickSkin(int avoid) {
        rng_ = rng_ * 1664525u + 1013904223u;
        int pick = static_cast<int>((rng_ >> 16) % (kChromaSkins - 1));
        if (pick >= avoid) ++pick;
        return static_cast<uint8_t>(pick);
    }

    uint32_t rng_ = 1;
    int goal_ = 1;
    int passes_ = 0;
    int round_ = 0;
    uint32_t window0_ = 0;      // the first round's window
    uint32_t windowStep_ = 0;   // ...and what each later round sheds off it
    uint32_t window_ = 0;       // this round's, floored at kChromaWindowFloorMs
    uint32_t elapsed_ = 0;      // into this round
    uint32_t wearMs_ = 0;       // since the last repaint started, capped at the settle
    uint8_t plate_ = 0;
    uint8_t worn_ = 0;
    uint8_t leaving_ = 0;       // what worn_ was before the live repaint started
    bool switching_ = false;    // may a round change the water under the pet at all
    bool switched_ = false;     // ...and has this one already
    State state_ = State::Running;
};

}  // namespace mal
