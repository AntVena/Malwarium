// maint_screen.h — MAINT submenu: a 2-row list (Defrag / AV) whose
// actions launch a non-interruptible process with a success/failure outcome.
#pragma once

#include "core/model/pet_model.h"
#include "core/model/stacker.h"

namespace mal {

class Framebuffer;

enum class MaintKind { Defrag, Av };

// Gated states: nothing to do, so the action is inert.
inline bool defragGated(const PetModel& m) { return m.fragmentation() == 0; }
inline bool avGated(const PetModel& m) {
    return m.debuffs() == 0 && !m.hasGhost() && m.fragmentation() == 0;
}

// L2: two rows previewing current Fragmentation / debuff+ghost status.
void drawMaintList(Framebuffer& fb, const PetModel& m, int cursor, int beat);

// L3 action screen: effect summary + Run/Scan (or the gated line).: a
// Defrag shows its stage-scaled Bits `cost` and gates on the `walletBits` balance
// (AV passes cost 0 — it stays free).: the Defrag screen offers three payment
// VARIANTS — `variant` 0 = QUICK (Bits, may fail), 1 = TOOL (a Defrag Tool item for a
// guaranteed clean), 2 = STACKER (the minigame, guaranteed if cleared); `toolCount` is
// how many tools are held; `defragCount` is this pet's running tally (surfaced, no
// mechanical effect). All ignored for AV.
void drawMaintAction(Framebuffer& fb, MaintKind kind, const PetModel& m,
                     int cost, int walletBits, int variant, int toolCount,
                     int defragCount);

// The Stacker board (variant 2). Draws the locked stack, the run currently in hand, and
// either the controls or the result. `frag` is the disk as it stands: context for what
// clearing the board would fix, and — since a cleared board wipes it — the win's own
// payout, which is why the screen can price the run without being told.
void drawStackerBoard(Framebuffer& fb, const Stacker& s, int frag);

// What a played board is worth in Fragmentation: kStackerScorePerFrag points to the
// point, or the whole disk if it was cleared. Shared by the board and its outcome toast
// so the number the player watched climb is the number they end up with.
int stackerFragWorth(const Stacker& s, int frag);

// Running process: a progress bar (C ignored until it resolves). `t` 0..1.
void drawMaintProcess(Framebuffer& fb, MaintKind kind, float t);

// What a finished run did to the disk. Failed and Cleaned are the only two a ROLLED or
// BOUGHT run can reach; Partial belongs to the played one, whose board can come up short
// without that being a failure — no penalty, no care mistake, just a smaller clean.
enum class MaintOutcome : uint8_t { Failed, Cleaned, Partial };

// Outcome toast after the process resolves. `fragRemoved` is what actually came off:
// the fixed kDefragReduction/kAvReduction for a rolled or bought run, the board's own
// worth for a played one — which is why the toast prints a number instead of implying
// one. Ignored on Failed, which paid kMaintFailPenalty instead.
void drawMaintOutcome(Framebuffer& fb, MaintKind kind, MaintOutcome outcome,
                      int fragRemoved);

} // namespace mal
