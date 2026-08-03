#include "core/content/areas/deepweb_dive/area.h"

#include "core/content/areas/area_defs.h"

namespace mal {

const int kDeepWebEnemyLevelOffset = 0;
const int kDeepWebHealthPerLevel = 6;
const int kDeepWebSpeedPerNLevels = 6;
const int kDeepWebDepthLevelPerLog2 = 2;
const int kDeepWebDepthBitsPctPerLog2 = 32;
const int kDeepWebDepthBitsMaxPct = 512;

// The dive's mod pool — earned from milestone wins in the endless zone, the
// defensive/build-around endgame Epics.
const char* const kAreaModsDeepWeb[] = {
    "deadman_switch", "raid_mirror",  "ecc_memory",       "load_balancer",
    "phishing_rod",    "extortion_ledger", "backup_uplink",
};
const int kAreaModsDeepWebCount = arrLen(kAreaModsDeepWeb);

}  // namespace mal
