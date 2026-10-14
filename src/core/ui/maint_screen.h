// maint_screen.h — MAINT submenu: a 2-row list (Defrag / AV) whose actions either
// launch a non-interruptible process with a success/failure outcome, or hand off to the
// process's played variant (whichever minigame content_arcade.h wires to it).
#pragma once

#include "core/content/content_arcade.h"
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

// The played variant's row as the action screen names it: the game wired to this
// process (content_arcade.h's kDefragPlayedGame / kAvPlayedGame), what one unit of it
// is, and where the shared difficulty dial stands.
struct PlayedRow {
    const char* name = "";
    const char* unit = "";
    const char* bitsUnit = nullptr;   // the game's Bits unit, if it has one
    ArcadeDifficulty difficulty = ArcadeDifficulty::Medium;
};

// L3 action screen: effect summary, the payment VARIANTS as a pick, and Run/Scan (or the
// gated line). `variant` is the focused one — DEFRAG: 0 = QUICK (Bits, may fail),
// 1 = TOOL (a Defrag Tool item for a guaranteed clean), 2 = PLAYED; AV: 0 = QUICK,
// 1 = PLAYED. A Defrag shows its stage-scaled Bits `cost` against `walletBits` (AV is
// free); `toolCount` is how many tools are held; `defragCount` is this pet's running
// tally. `failPct` is the chance a QUICK run fails right now (Game::maintFailPct), drawn
// beside it with what a failure costs.
void drawMaintAction(Framebuffer& fb, MaintKind kind, const PetModel& m,
                     int cost, int walletBits, int variant, int toolCount,
                     int defragCount, int failPct, const PlayedRow& played);

// The live readout a played board's footer shows, filled in by Game so neither board
// knows which context it is being played in. `title` heads the board (the cabinet's
// name, or the MAINT process's). A cabinet reads `score` against `scoreMax`; a MAINT run
// reads the disk as it will stand (`fragNow`), what the run has taken off it so far
// (`fragOff`) and what it has earned (`bits`, shown whenever the process `paysBits`).
struct PlayHud {
    const char* title = "";
    bool arcade = false;
    int score = 0;
    int scoreMax = 0;
    int fragNow = 0;
    int fragOff = 0;
    int bits = 0;
    bool paysBits = false;
};

// The footer's right column, shared by every played board.
void drawPlayHudWorth(Framebuffer& fb, const PlayHud& hud);

// The Stacker board. Draws the locked stack, the run currently in hand, and either the
// controls or the result, over the shared footer.
void drawStackerBoard(Framebuffer& fb, const Stacker& s, const PlayHud& hud);

// The FRAGGER board: the lanes, the falling blocks, the scan head and the footer. The
// shot is the last one fired, drawn as a beam up its lane until the next row falls (-1
// lane for none; Fragger::fire for what `shotRow` means).
void drawFraggerBoard(Framebuffer& fb, const Fragger& f, const PlayHud& hud, int shotLane,
                      int shotRow);

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
