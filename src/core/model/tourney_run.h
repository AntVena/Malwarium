// tourney_run.h: one ROCK THE DOCK bracket, a run (CONTRIBUTING, "Where state lives").
// From the draw on the EXPL arena row until the operator dismisses its verdict. Each
// bout is its own Fight; this holds the bracket between them. Every entrant is derived
// from the seed (core/model/tournament.h), so the run is four bytes plus the opponent it
// has seated. It survives a reboot, so it maps itself to the save (SaveTourney) the way
// a record does.
//
// Game holds it only while a bracket is in play, verdict included: dismissing the
// verdict drops it.
#pragma once

#include <cstdint>
#include <optional>

#include "core/model/save.h"
#include "core/model/tournament.h"
#include "tunables.h"

namespace mal {

// Ready is a bracket still being fought. The other two are terminal banners the operator
// has yet to dismiss, and they persist, so a device put down on the verdict still shows
// it after a reboot.
enum class TourneyPhase : uint8_t { Ready, Eliminated, Champion };

struct TourneyRun {
    uint32_t seed = 1;   // never 0: a zero seed in the save means no run
    uint8_t alive = 0;   // survivor bitmask over kTourneySlots
    uint8_t round = 0;   // 0..kTourneyRounds-1
    TourneyPhase phase = TourneyPhase::Ready;
    // The next bout's opponent, built from the seed rather than saved. Building one walks
    // the move and mod tables, so it is seated once per round (seatOpponent) and the
    // bracket screen and the bout both read the held copy.
    TourneyFighter opponent;

    // A fresh bracket: everyone standing, round one. The caller resolves the round's
    // other pairings (tourneyResolveRound) before the operator reads the field.
    static TourneyRun drawn(uint32_t seed) {
        TourneyRun r;
        r.seed = seed ? seed : 1u;
        r.alive = static_cast<uint8_t>((1u << kTourneySlots) - 1);
        return r;
    }

    int playerSlot() const { return tourneyPlayerSlot(seed); }
    // The slot the operator faces next, or -1 once the run has a verdict.
    int opponentSlot() const {
        if (phase != TourneyPhase::Ready || round >= kTourneyRounds) return -1;
        return tourneyOpponentSlot(alive, playerSlot(), round);
    }
    void seatOpponent(const ContentRegistry& reg) {
        const int opp = opponentSlot();
        opponent = opp >= 0 ? tourneyEntrant(reg, seed, opp) : TourneyFighter{};
    }

    SaveTourney saved() const {
        SaveTourney s;
        s.seed = seed;
        s.alive = alive;
        s.round = round;
        s.phase = static_cast<uint8_t>(phase);
        return s;
    }
    // A zero seed is no run. A round past the last is held on the last, and a phase byte
    // this build does not have reads as Ready, so a stale save resumes a fightable run
    // rather than a screen stuck on a verdict it cannot dismiss. The opponent is not
    // seated here: the bracket screen seats it on entry.
    static std::optional<TourneyRun> loaded(const SaveTourney& s) {
        if (s.seed == 0) return std::nullopt;
        TourneyRun r;
        r.seed = s.seed;
        r.alive = s.alive;
        r.round = s.round < kTourneyRounds ? s.round : static_cast<uint8_t>(kTourneyRounds - 1);
        r.phase = s.phase <= static_cast<uint8_t>(TourneyPhase::Champion)
                      ? static_cast<TourneyPhase>(s.phase)
                      : TourneyPhase::Ready;
        return r;
    }
};

}  // namespace mal
