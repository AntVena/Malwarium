// content_evolution.cpp — egg lines + evolution routing (EggLineDef/SignalRouteDef/DaemonPoolDef).
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

// Evolution routing tables ----------------------------------
// The branching mechanism as pure data. Placeholder rows: every dominant signal
// maps to the current single linear successor, and each Daemon pool holds one
// entry — so hatch still yields CryptoShell->Paypup->Malbear->Bruinforce. A
// later names/lines pass fans the signal columns out and grows the Daemon pools
// without touching evolve logic. Consumed by Game::evolutionTargetId.
//
// bySignal order matches DominantSignal: {Feeding, Play, Maintenance, Training, Balanced}.
const SignalRouteDef kSignalRoutes[] = {
    // CryptoShell (Boot) -> Paypup (Process).
    {"cryptoshell", {"paypup", "paypup", "paypup", "paypup", "paypup"}},
    // Paypup (Process) -> Malbear (Script).
    {"paypup", {"malbear", "malbear", "malbear", "malbear", "malbear"}},
};
const int kSignalRoutesCount = sizeof(kSignalRoutes) / sizeof(kSignalRoutes[0]);

const DaemonPoolDef kDaemonPools[] = {
    // Malbear (Script) -> Daemon, per care-branch. Single-entry pools today.
    {"malbear", /*badBranch=*/false, {{"bruinforce", 1}}, 1},
    {"malbear", /*badBranch=*/true, {{"berserkernel", 1}}, 1},
};
const int kDaemonPoolsCount = sizeof(kDaemonPools) / sizeof(kDaemonPools[0]);

}  // namespace mal
