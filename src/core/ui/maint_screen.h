// maint_screen.h — MAINT submenu: a 2-row list (Defrag / AV) whose actions either
// launch a non-interruptible process with a success/failure outcome, or hand off to the
// process's played variant (FRAGGER for Defrag, the Stacker firewall for AV).
#pragma once

#include "core/model/fragger.h"
#include "core/model/pet_model.h"
#include "core/model/stacker.h"
#include "core/ui/ui_state.h"  // MaintKind

namespace mal {

class Framebuffer;

// Gated states: nothing to do, so the action is inert.
inline bool defragGated(const PetModel& m) { return m.fragmentation() == 0; }
inline bool avGated(const PetModel& m) {
    return m.debuffs() == 0 && !m.hasGhost() && m.fragmentation() == 0;
}

// L2: two rows previewing current Fragmentation / debuff+ghost status.
void drawMaintList(Framebuffer& fb, const PetModel& m, int cursor, int beat);

// L3 action screen: effect summary, the payment VARIANTS as a pick, and Run/Scan (or the
// gated line). `variant` is the focused one — DEFRAG: 0 = QUICK (Bits, may fail),
// 1 = TOOL (a Defrag Tool item for a guaranteed clean), 2 = FRAGGER (the minigame,
// paid per fragment shot); AV: 0 = QUICK, 1 = STACKER (the firewall, cannot fail). A
// Defrag shows its stage-scaled Bits `cost` against `walletBits` (AV is free);
// `toolCount` is how many tools are held; `defragCount` is this pet's running tally.
// `failPct` is the chance a QUICK run fails right now (Game::maintFailPct), drawn beside
// it with what a failure costs.
void drawMaintAction(Framebuffer& fb, MaintKind kind, const PetModel& m,
                     int cost, int walletBits, int variant, int toolCount,
                     int defragCount, int failPct = 0);

// The Stacker board (AV's played variant, and its cabinet). Draws the locked stack, the
// run currently in hand, and either the controls or the result. `frag` is the disk as it
// stands: context for what the wall's blocks are worth off it. `arcade` swaps the
// right-hand readout: a scan is counting Fragmentation off a disk, and a cabinet run has
// no disk, so it counts the raw score the arcade will actually pay on instead.
void drawStackerBoard(Framebuffer& fb, const Stacker& s, int frag, bool arcade);

// What a played scan's board is worth in Fragmentation: kStackerScorePerFrag points to
// the point, or a full scan's kAvReduction if the wall was built, never more than that
// or than the disk holds. Shared by the board and finishStacker so the number the player
// watched climb is the number they end up with.
int stackerScanWorth(const Stacker& s, int frag);

// What drawFraggerBoard needs that the run itself doesn't hold. `arcade` swaps the
// right-hand readout the same way the Stacker board's does; `frag` is the disk the run
// is shooting off, before any of it is paid; the shot is the last one fired, drawn as a
// beam up its lane until the next row falls (-1 lane for none; Fragger::fire for what
// `row` means).
struct FraggerView {
    bool arcade = false;
    int frag = 0;
    int shotLane = -1;
    int shotRow = -1;
};

// FRAGGER (Defrag's played variant, and its cabinet): the seven lanes, the falling
// blocks, the scan head and the live HUD.
void drawFraggerBoard(Framebuffer& fb, const Fragger& f, const FraggerView& v);

// What a FRAGGER run has taken off a disk of `frag`: kFraggerFragPerHit per fragment
// shot, never more than the disk holds. Shared by the HUD and finishFragger, so the
// number the player watched count down is the one they end up with.
int fraggerFragWorth(const Fragger& f, int frag);

// Running process: the disk map for that process animating over a progress bar (C
// ignored until it resolves). `t` 0..1.
void drawMaintProcess(Framebuffer& fb, MaintKind kind, float t);

// What a finished run did to the disk. Failed and Cleaned are the only two a ROLLED or
// BOUGHT run can reach; Partial belongs to the played one, whose board can come up short
// without that being a failure — no penalty, no care mistake, just a smaller clean.
enum class MaintOutcome : uint8_t { Failed, Cleaned, Partial };

// Outcome toast after the process resolves. `fragRemoved` is what actually came off:
// the fixed kDefragReduction/kAvReduction for a rolled or bought run, the run's own
// worth for a played one — which is why the toast prints a number instead of implying
// one. Ignored on Failed, which paid kMaintFailPenalty instead. `bitsEarned` is what a
// played defrag paid on top, drawn only when there is some.
void drawMaintOutcome(Framebuffer& fb, MaintKind kind, MaintOutcome outcome,
                      int fragRemoved, int bitsEarned = 0);

} // namespace mal
