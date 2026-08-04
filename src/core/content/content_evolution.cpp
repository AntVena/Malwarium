// content_evolution.cpp — egg lines + evolution routing (EggLineDef/DaemonPoolDef).
//
// One content table (see content_tables.h). Edit rows here; the registry picks
// them up unchanged via embedded_content.cpp.
#include "core/content/content_tables.h"

namespace mal {

const EggLineDef kEggLines[] = {
    {"ransomware", "Ransomware", "cryptoshell", HatchGame::Decrypt},
    // A second baseline line (model): its own Boot-Sector egg, so a
    // fresh save's line-select offers Ransomware vs Phishing. Both unlocked
    // from the start in v1 (no unlock gating yet); the modal auto-skips when only one
    // line exists, so with only one line the boot drops straight into CryptoShell.
    // Its egg is laid into a clutch of decoys, so the hatch is a Clutch Pick played
    // once at lay-time rather than a decrypt grind against the incubation clock.
    {"phishing", "Phishing", "phrogspawn", HatchGame::Clutch},
};
const int kEggLinesCount = sizeof(kEggLines) / sizeof(kEggLines[0]);

// Script->Daemon weighted pools, per care-branch. This is the one routing decision a
// creature row cannot express on its own: a pool can hold several Daemons and draw
// between them by weight, where CreatureDef only has room for one Good and one Bad.
// A Script with no pool here falls through to its own evolvesToGoodId/BadId, which is
// what every chain but Malbear's does. Consumed by Game::evolutionTargetId.
const DaemonPoolDef kDaemonPools[] = {
    // Malbear (Script) -> Daemon. Single-entry pools today.
    {"malbear", /*badBranch=*/false, {{"bruinforce", 1}}, 1},
    {"malbear", /*badBranch=*/true, {{"berserkernel", 1}}, 1},
};
const int kDaemonPoolsCount = sizeof(kDaemonPools) / sizeof(kDaemonPools[0]);

}  // namespace mal
