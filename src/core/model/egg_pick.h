// egg_pick.h: one Clutch Pick, a run (CONTRIBUTING, "Where state lives"). From the
// moment a Phishing egg is laid (or the Clutch cabinet starts) until B dismisses its
// reveal. The live egg hides among decoys in a kEggPickCols x kEggPickRows raft, and the
// player halves the raft `rounds` times; finishing with the live egg still in the span
// wins. Runtime-only and never saved: the whole game is played in the seconds after the
// egg is laid, so a reboot mid-pick forfeits the bonus and leaves a normal egg.
//
// Game holds it only while the pick is on screen, reveal included. The raft's geometry
// and its draw are the modal's (game_eggpick.cpp): this is the rules.
#pragma once

#include <cstdint>

namespace mal {

constexpr int kEggPickRounds = 3;   // the hatch's halvings; a cabinet's dial picks its own
constexpr int kEggPickCols = 8;     // the clutch grid BG_EGG_CLUTCH bakes
constexpr int kEggPickRows = 4;

struct EggPick {
    uint8_t target = 0;       // slot 0..kEggPickCols*kEggPickRows-1 holding the live egg
    uint8_t rounds = kEggPickRounds;   // halvings this pick asks for
    uint8_t round = 0;        // halvings committed so far
    uint8_t col = 0;          // surviving column span [col, col + colSpan)
    uint8_t colSpan = kEggPickCols;
    uint8_t row = 0;          // surviving row span [row, row + rowSpan)
    uint8_t rowSpan = kEggPickRows;
    bool secondHalf = false;  // the C half (right / bottom) is aimed at, vs the A half
    bool resolved = false;    // rounds done: the reveal, waiting on B
    bool won = false;

    static EggPick started(int target, int rounds) {
        EggPick p;
        p.target = static_cast<uint8_t>(target);
        p.rounds = static_cast<uint8_t>(rounds < 1 ? 1 : rounds);
        return p;
    }

    bool targetInSpan() const {
        const int c = target % kEggPickCols, r = target / kEggPickCols;
        return c >= col && c < col + colSpan && r >= row && r < row + rowSpan;
    }

    // Which axis this round halves. Rounds alternate columns and rows so the surviving
    // span stays as square as it can, falling back to the other axis if one is already
    // down to a single track (a guard, not a case the shipped 8x4, 3-round shape reaches).
    bool splitsColumns() const {
        if (colSpan < 2) return false;
        if (rowSpan < 2) return true;
        return (round % 2) == 0;
    }

    // A/C. False when nothing changed: the same half again, or a reveal, which is a
    // read-only screen that only B leaves.
    bool aim(bool second) {
        if (resolved || secondHalf == second) return false;
        secondHalf = second;
        return true;
    }

    // B on a running pick: keep the aimed half. The pick resolves on the turn the live egg
    // falls outside the span, rather than playing out the rounds it has left, or at the
    // last round with the egg still inside.
    void commit() {
        if (splitsColumns()) {
            const uint8_t half = colSpan / 2;
            if (secondHalf) col = static_cast<uint8_t>(col + half);
            colSpan = half;
        } else {
            const uint8_t half = rowSpan / 2;
            if (secondHalf) row = static_cast<uint8_t>(row + half);
            rowSpan = half;
        }
        ++round;
        secondHalf = false;   // every round re-opens aimed at its first half
        if (!targetInSpan()) {
            resolved = true;
            won = false;
        } else if (round >= rounds) {
            resolved = true;
            won = true;
        }
    }
};

}  // namespace mal
