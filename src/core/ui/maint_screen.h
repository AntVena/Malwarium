// maint_screen.h — MAINT submenu: a 2-row list (Defrag / AV) whose
// actions launch a non-interruptible process with a success/failure outcome.
#pragma once

#include "core/model/pet_model.h"

namespace mal {

class Framebuffer;

enum class MaintKind { Defrag, Av };

// Gated states: nothing to do, so the action is inert.
inline bool defragGated(const PetModel& m) { return m.fragmentation() == 0; }
inline bool avGated(const PetModel& m) {
    return m.debuffs() == 0 && !m.hasGhost() && m.fragmentation() == 0;
}

// L2: two rows previewing current Fragmentation / debuff+ghost status.
void drawMaintList(Framebuffer& fb, const PetModel& m, int cursor);

// L3 action screen: effect summary + Run/Scan (or the gated line).: a
// Defrag shows its stage-scaled Bits `cost` and gates on the `walletBits` balance
// (AV passes cost 0 — it stays free).: the Defrag screen offers two payment
// VARIANTS — `variant` 0 = QUICK (Bits, may fail), 1 = TOOL (a Defrag Tool item for a
// guaranteed clean); `toolCount` is how many tools are held; `defragCount` is this
// pet's running tally (surfaced, no mechanical effect). All ignored for AV.
void drawMaintAction(Framebuffer& fb, MaintKind kind, const PetModel& m,
                     int cost, int walletBits, int variant, int toolCount,
                     int defragCount);

// Running process: a progress bar (C ignored until it resolves). `t` 0..1.
void drawMaintProcess(Framebuffer& fb, MaintKind kind, float t);

// Outcome toast after the process resolves.
void drawMaintOutcome(Framebuffer& fb, MaintKind kind, bool success);

} // namespace mal
