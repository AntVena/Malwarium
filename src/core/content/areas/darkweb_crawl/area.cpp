#include "core/content/areas/darkweb_crawl/area.h"

#include "core/content/areas/area_defs.h"

namespace mal {

// These are the DeepWeb Dive's own numbers from before it moved to Net-Sea and was
// softened for the pet that arrives there. They are kept together here rather than
// re-derived because they are MEASURED: the win-rate-by-build table on
// deepweb_dive/area.cpp's rung pacing describes this curve, and the Crawl is where that
// measurement still applies.
const int kDarkWebEnemyLevelOffset = 0;
const int kDarkWebDepthLevelPerLog2 = 2;
const int kDarkWebBudgetPct = 100;
const int kDarkWebDepthPointsPerN = 8;
const int kDarkWebDepthBitsPctPerLog2 = 48;
const int kDarkWebDepthBitsMaxPct = 768;

// Every 16 wins deep. Deliberately slower than the radio: a handshake is one capture and
// this is sixteen fights with no override, so the Crawl never devalues the aerial — it
// just means a player without one is not locked out of the Cant forever.
const int kDarkWebSigilEveryN = 16;

const char* const kDarkWebIcon = "ICON_SECTOR_DARKWEB_CRAWL";

// The terminal zone's shelf: the Lode's own rank-6 set, plus the Dive's hard-gated line
// build-arounds. Reaching it opens the deep end of every line at once, which is what the
// last zone on the map owes a player who got here without being able to read the menu.
const char* const kAreaModsDarkWeb[] = {
    "crib_sheet",     "vault_door",       "spoof_relay",    "fork_farm",
    "false_flag",     "recompiler",       "phishing_rod",   "extortion_ledger",
    "ring_zero_shim", "replication_bus",  "mutation_engine",
};
const int kAreaModsDarkWebCount = arrLen(kAreaModsDarkWeb);

// The staple set, plus the five unbuyable USBs the Dive used to be the only source of.
// They move here with the terminal slot: the price of overruling how a pet was raised is
// a trip to the end of the map, and the end of the map is no longer the Dive.
const LootEntry kWildLootDarkWeb[] = {
    {"dyno_nuggets"}, {"tortilla_chip"}, {"backup_drive"}, {"disk_scrubber"},
    {"pwnzu_sauce"},
    {"sandbox_usb"}, {"bad_usb"}, {"signed_usb"}, {"halt_usb"}, {"eject_usb"},
};
const int kWildLootDarkWebCount = arrLen(kWildLootDarkWeb);

}  // namespace mal
