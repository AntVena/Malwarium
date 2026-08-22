// content_evolution.cpp — egg lines + evolution routing (EggLineDef/DaemonPoolDef).
//
// One content table (see content_tables.h). Edit rows here; the registry picks
// them up unchanged via embedded_content.cpp.
#include "core/content/content_achievements.h"
#include "core/content/content_tables.h"

namespace mal {

const EggLineDef kEggLines[] = {
    {"ransomware", "Ransomware", "cryptoshell", HatchGame::Decrypt},
    // A second line with its own Boot-Sector egg, so line-select offers Ransomware
    // vs Phishing — but only once it is EARNED: Game::eggLineUnlocked hides it until
    // the first DeepWeb-depth milestone, so a fresh save sees one line and the modal
    // auto-skips straight into CryptoShell. Its egg is laid into a clutch of decoys,
    // so the hatch is a Clutch Pick played once at lay-time rather than a decrypt
    // grind against the incubation clock.
    {"phishing", "Phishing", "phrogspawn", HatchGame::Clutch, ach::kDeepWebDepth8},
    // The third egg line, and the one that has to be EARNED by replicating rather than
    // by diving: Game::eggLineUnlocked hides it until the archive holds two of the same
    // species at once (ach::kSecondInstance). Its hatch is the Isolation Protocol, played
    // once at lay-time like the Clutch — a worm loose in a quarantine buffer, eating the
    // incubation clock a minute at a time.
    {"worm", "Worm", "vermicell", HatchGame::Isolation, ach::kSecondInstance},
    // The fourth egg line, earned by the MIRROR: the operator's pet facing its own
    // species, over the LINK or in the arena (ach::kHashCollision). Every earned line
    // asks for the kind of play it is about, and this is the sharpest of the four — a
    // roster of distinct silhouettes exists so you can tell one creature from another,
    // and the one moment that fails is the one that earns the line about being
    // indistinguishable. Leaving it ungated is not a free choice: an ungated fourth row
    // puts TWO lines in front of a fresh save, turning the opening auto-skip into a
    // line-select modal, which test_creature_lines.cpp holds.
    //
    // Its hatch is the CHROMATOPHORE (game_chroma.cpp): the bell rehearses wearing three
    // other colours against a sweep, one skin per button. The line's own argument as a
    // board — every other hatch game asks the player to FIND something, and this one
    // asks them to become something in time, which is the one thing this family does
    // that no other does.
    {"metamorphic", "Metamorphic", "polystaria", HatchGame::Chroma, ach::kHashCollision},
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
