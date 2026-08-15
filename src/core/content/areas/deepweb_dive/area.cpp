#include "core/content/areas/deepweb_dive/area.h"

#include "core/content/areas/area_defs.h"

namespace mal {

const int kDeepWebEnemyLevelOffset = 0;
const int kDeepWebHealthPerLevel = 6;
const int kDeepWebSpeedPerNLevels = 6;
const int kDeepWebDepthLevelPerLog2 = 2;
const int kDeepWebDepthBitsPctPerLog2 = 32;
const int kDeepWebDepthBitsMaxPct = 512;

// The dive's EXPL row glyph. The terminal zone is no AreaDef, so it names its own
// art here rather than inheriting the ICON_SECTOR_<AREA_ID> convention off a row it
// doesn't have — same reason its pool and scaling constants live here.
const char* const kDeepWebIcon = "ICON_SECTOR_DEEPWEB_DIVE";

// The dive's mod pool — earned from milestone wins in the endless zone, the
// defensive/build-around endgame Epics.
const char* const kAreaModsDeepWeb[] = {
    "deadman_switch", "raid_mirror",  "ecc_memory",       "load_balancer",
    "phishing_rod",    "extortion_ledger", "backup_uplink",
};
const int kAreaModsDeepWebCount = arrLen(kAreaModsDeepWeb);

// The dive's WILD-win drop table. Same staple set every area's row carries, plus the
// dive's own exclusive Merge Hub ingredient — the terminal zone is no AreaDef, so its
// pool is named here for the same reason its glyph and scaling constants are.
const LootEntry kWildLootDeepWeb[] = {
    {"airgap_snack"}, {"tortilla_chip"}, {"backup_drive"}, {"disk_scrubber"},
    {"pwnzu_sauce"},
};
const int kWildLootDeepWebCount = arrLen(kWildLootDeepWeb);

}  // namespace mal
